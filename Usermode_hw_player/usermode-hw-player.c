#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <limits.h>
#include <sys/mman.h>

/*
FIFO information
base addy = c_baseaddr
cut through = enabled
fifo depth = 1024
program full thresh = 512
program empty thresh = 1
*/
#define REG_OFFSET(REG, OFF) ((REG) + (OFF))
#define SND_CARD "default"

// pg 19 for registers
unsigned long C_BASEADDR = 0x41630000;
unsigned int ISR = 0x0;   // Interrupt status register
unsigned int IER = 0x4;   // interrupt enable register
unsigned int TDFR = 0x8;  // transmid data fifo reset
unsigned int TDFV = 0xC;  // transmit data fifo vacancy
unsigned int TDFD = 0x10; //
unsigned int TLR = 0x14;  // Transmit length register pg 27-28
unsigned int RDFO = 0x1C; // Transmit length register pg 27-28
unsigned int TDR = 0x2C;  // tansmit dest reg

unsigned long AxiSampleRate = 44100;
unsigned int shift = 0;
unsigned char *audioRegs; // pointer for reading / writing

// NOTE use sizes from STDINT
// NOTE verify data alignment!
struct wave_header // ednian-ness to right of var
{
    // RIFF CHUNK
    uint32_t ChunkID;   // big
    uint32_t ChunkSize; // lil
    uint32_t Format;    // big

    // FMT sub-chunk
    uint32_t Subchunk1ID;   // big
    uint32_t Subchunk1Size; // lil
    uint16_t AudioFormat;   // lil
    uint16_t NumChannels;   // lil
    uint32_t SampleRate;    // lil
    uint32_t ByteRate;      // lil
    uint16_t BlockAlign;    // lil
    uint16_t BitsPerSample; // lil

    // DATA sub-chunk
    uint32_t Subchunk2ID;   // big
    uint32_t Subchunk2Size; // lil
                            // data is not in header
};

int configure_codec(unsigned int sample_rate,
                    snd_pcm_format_t format,
                    snd_pcm_t *handle,
                    snd_pcm_hw_params_t *params)
{
    int err;

    // initialize parameters
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0)
    {
        printf("Failed to initialize parameters");
        return err;
    }

    // TODO: set format
    err = snd_pcm_hw_params_set_format(handle, params, format);
    if (err < 0)
    {
        printf("Failed to initialize parameters");
        return err;
    }

    // NOTE: the codec only supports one audio format, this should be constant
    //       and not read from the WAVE file. You must convert properly to this
    //       format, regardless of the format in your WAVE file
    //       (bits per sample and alignment).

    // set channel count
    err = snd_pcm_hw_params_set_channels(handle, params, 2);
    if (err < 0)
    {
        printf("Failed to set channels");
        return err;
    }

    // set sample rate
    err = snd_pcm_hw_params_set_rate_near(handle, params, &sample_rate, 0);
    if (err < 0)
    {
        printf("Failed to set sample rate");
        return err;
    }

    // TODO: write parameters to device
    err = snd_pcm_hw_params(handle, params);
    if (err < 0)
    {
        printf("Failed to write parameters to device");
        return err;
    }

    return 0;
}

void pr_usage(char *pname)
{
    printf("usage: %s WAV_FILE\n", pname);
}

/* @brief Read WAVE header
   @param fp file pointer
   @param dest destination struct
   @return 0 on success, < 0 on error */
int read_wave_header(FILE *fp, struct wave_header *dest)
{
    if (!dest || !fp)
    {
        return -ENOENT;
    }

    fseek(fp, 0, SEEK_SET); // Ensure we are at the start of the file

    if (fread(dest, sizeof(struct wave_header), 1, fp) != 1)
    {
        return -EIO; // Error in reading file
    }

    return 0;
    // NOTE do not assume file pointer is at its starting point
}

/* @brief Parse WAVE header and print parameters
   @param hdr a struct wave_header variable
   @return 0 on success, < 0 on error or if not WAVE file*/
int parse_wave_header(struct wave_header hdr)
{
    // TODO verify that this is a RIFF file header
    if (hdr.ChunkID != 0x52494646)
    {
        printf("ChunkID does not allign w expected value \n");
        printf("ChunkID: %i\n", hdr.ChunkID);
        return -errno;
    }

    // TODO verify that this is WAVE file
    if (hdr.Format != 0x57415645)
    {
        printf("Format is not a wave file\n");
        return -errno;
    }

    // TODO verify that this is WAVE file
    if (hdr.AudioFormat != 0x1)
    {
        printf("Format is not a wave file\n");
        return -errno;
    }

    // print out info abt wav file
    // printf("The total size in bytes is: %u\n", 44 + hdr.Subchunk2Size); // lil
    // printf("The total number of channels are: %x\n", hdr.NumChannels);  // lil
    // printf("The Sample-rate is are: %d Hz\n", hdr.SampleRate);          // lil
    // printf("The bits per sample is: %x\n", hdr.BitsPerSample);

    return 0;
}

/* @brief Check if FIFO is full
   @return 0 if not full, "true" otherwise */
unsigned char fifo_full(void)
{
    // pg 21 & 22
    //  within ISR
    int transmit_fifo_full = 1 << 22;

    if ((*(volatile unsigned int *)REG_OFFSET(audioRegs, TDFV)) == 0x0)
    {
        return 1; // full
    }

    return 0; // not full
}

/* @brief Transmit a word (put into FIFO)
   @param word a 32-bit word */
void fifo_transmit_word(int fd, uint32_t word)
{
    if (write(fd, &word, sizeof(word)) < 0)
    {
        printf("%sWrite to character device failed");
    }
}

/* @brief Build a 32-bit audio word from a buffer
   @param hdr WAVE header
   @param buf a byte array
   @return 32-bit word */
uint32_t audio_word_from_buf(struct wave_header hdr, uint8_t *buf)
{
    uint32_t audio_word = 0;
    int i;

    switch (hdr.BitsPerSample)
    {
    case 8:
    {
        // 8-bit samples
        uint8_t sample = buf[0];

        // Convert to 24-bit
        audio_word = ((uint32_t)(sample - 127)) << 24;
        break;
    }
    case 16:
    {
        // 16-bit samples
        int16_t sample = *((int16_t *)buf);
        // Convert to 24-bit
        audio_word = ((uint32_t)sample) << 16;
        break;
    }

    case 24:
    {
        // 24-bit samples
        int32_t sample = *((int32_t *)buf);
        // Convert to 24-bit
        audio_word = ((uint32_t)sample) << 8;
        break;
    }

    default:
        printf("Unsupported BitsPerSample: %d\n", hdr.BitsPerSample);
    }

    return audio_word;
}

/* @brief Play sound samples
   @param fp file pointer
   @param hdr WAVE header
   @param sample_count how many samples to play or -1 plays to end of file
   @param start starting point in file for playing
   @return 0 if successful, < 0 otherwise */
int play_wave_samples(FILE *fp, struct wave_header hdr, int sample_count, unsigned int start, int fd)
{
    int bytesPerSample = hdr.BitsPerSample / 8;
    int frameSize = bytesPerSample * hdr.NumChannels;
    uint8_t *buffer = malloc(frameSize);
    if (!buffer)
    {
        return -ENOMEM;
    }

    fseek(fp, sizeof(struct wave_header) + start, SEEK_SET);

    long samplesPlayed = 0;
    unsigned long samplesToRead = sample_count == -1 ? hdr.Subchunk2Size / frameSize : sample_count;

    while (samplesPlayed < samplesToRead)
    {
        if (fread(buffer, frameSize, 1, fp) < 1)
        {
            break; // End of file or read error
        }

        for (int i = 0; i < frameSize; i += bytesPerSample)
        {
            uint32_t word = audio_word_from_buf(hdr, &buffer[i]);
            if (hdr.NumChannels == 1)
            {
                // For mono files, duplicate the word to simulate stereo
                fifo_transmit_word(fd, word);
                fifo_transmit_word(fd, word);
            }
            else
            {
                // For stereo files, write each word once
                fifo_transmit_word(fd, word);
            }
        }
        samplesPlayed += (hdr.NumChannels == 1 ? frameSize / bytesPerSample * 2 : frameSize / bytesPerSample);
    }

    free(buffer);
    return 0;
}

static int map_audio(unsigned char **audioRegs, unsigned long gpio_addr)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    size_t size = 64 * sizeof(uint32_t);

    if (!audioRegs || !gpio_addr)
    {
        // invalid pointers
        return 1;
    }

    void *addr = mmap(0, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, gpio_addr);
    *audioRegs = (unsigned char *)addr;

    // return non-zero codes on errors!
    // success
    return 0;
}

int i2s_enable_tx(void)
{
    int fd;
    int err;
    char *buf = "1";

    fd = open("/sys/devices/soc0/amba_pl/77600000.axi_i2s_adi/tx_enabled", O_RDWR | O_SYNC);

    if (fd == -1)
    {
        printf("Open operation failed - enable");
        return -1;
    }

    err = write(fd, buf, 1);

    if (err == -1)
    {
        printf("Close operation failed - enable");
        return -1;
    }

    err = close(fd);
    if (err == -1)
    {
        printf("Close operation failed - enable");
        return -1;
    }

    return 0;
}

int i2s_disable_tx(void)
{
    int fd;
    int err;
    char *buf = "0";

    fd = open("/sys/devices/soc0/amba_pl/77600000.axi_i2s_adi/tx_enabled", O_RDWR | O_SYNC);

    if (fd == -1)
    {
        printf("Open operation failed - disable");
        return -1;
    }

    err = write(fd, buf, 1);

    if (err == -1)
    {
        printf("Close operation failed - enable");
        return -1;
    }

    err = close(fd);

    if (err == -1)
    {
        printf("Close operation failed - enable");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    FILE *fp;
    struct wave_header hdr;
    unsigned int totalSeconds;
    unsigned int SecIntoFile;
    unsigned int SecOffset;

    snd_pcm_t *handle;
    snd_pcm_hw_params_t *hwparams;
    int err;
    // placeholder variables, use values you read from your WAVE file
    unsigned int sample_rate;
    snd_pcm_format_t sound_format;

    // try to setup audios
    if (map_audio(&audioRegs, C_BASEADDR))
    {
        // failed to setup
        return 1;
    }

    // clears ISR
    *(volatile unsigned int *)REG_OFFSET(audioRegs, ISR) = 0xFFFFFFFF;

    // check number of arguments
    if (argc < 2)
    {
        // fail, print usage
        pr_usage(argv[0]);
        return 1;
    }

    // TODO open file
    fp = fopen(argv[1], "r");

    // TODO read file header
    err = read_wave_header(fp, &hdr);
    if (err != 0)
    {
        printf("Wave header was not sucessfuly read\n");
    }

    sample_rate = hdr.SampleRate;
    sound_format = hdr.AudioFormat;

    err = i2s_enable_tx();
    if (err < 0)
    {
        printf("i2s was not enabled sucessfully");
    }

    // allocate HW parameter data structures
    snd_pcm_hw_params_alloca(&hwparams);

    // open device (TX)
    err = snd_pcm_open(&handle, SND_CARD, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0)
    {
        printf("Device opening failed");
        return -1;
    }

    err = configure_codec(sample_rate, sound_format, handle, hwparams);
    if (err < 0)
    {
        printf("Configure codec failed");
        return -1;
    }

    // Open the character device
    int fd = open("/dev/zedaudio0", O_WRONLY);
    if (fd < 0)
    {
        printf("%sFailed to open the character device");
        return -1;
    }

    // play file
    play_wave_samples(fp, hdr, -1, 0, fd);

    close(fd);

    // TODO cleanup, uninitialize
    fclose(fp);
    snd_pcm_close(handle);

    err = i2s_disable_tx();
    if (err < 0)
    {
        printf("i2s was not enabled sucessfully");
    }

    return 0;
}