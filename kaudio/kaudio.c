/* LAB4 AUDIO WITH FIFO */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <asm/spinlock.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <asm/io.h>

// audio mapping address and offsets
//  pg 19 for registers
unsigned long C_BASEADDR = 0x41630000;
unsigned int ISR = 0x0;   // Interrupt status register
unsigned int IER = 0x4;   // interrupt enable register
unsigned int TDFR = 0x8;  // transmid data fifo reset
unsigned int TDFV = 0xC;  // transmit data fifo vacancy
unsigned int TDFD = 0x10; //
unsigned int TLR = 0x14;  // Transmit length register pg 27-28
unsigned int RDFO = 0x1C; // Transmit length register pg 27-28
unsigned int TDR = 0x2C;  // tansmit dest reg
unsigned char *audioRegs_global;

// tx interrput errors
unsigned int TX_FULL = 22;
unsigned int TX_SIZE_ERR = 25;
unsigned int TX_OVERRUN_ERR = 28;
unsigned int TX_EMPTY = 21;

#define DRIVER_NAME "esl-audio"

// forward declaration of axi_i2s
struct axi_i2s;

// declare functions we will use from the esli2s driver
extern void esl_codec_enable_tx(struct axi_i2s *i2s);
extern void esl_codec_disable_tx(struct axi_i2s *i2s);

// structure for an individual instance (ie. one audio driver)
struct esl_audio_instance
{
  void *__iomem regs;  // fifo registers
  struct cdev chr_dev; // character device
  dev_t devno;

  // list head
  struct list_head inst_list;

  // interrupt number
  unsigned int irqnum;

  // fifo depth
  unsigned int tx_fifo_depth;

  // fifo buffer
  unsigned char fifo_buf[31];

  // wait queue
  wait_queue_head_t waitq;

  // i2s controller instance
  struct axi_i2s *i2s_controller;
};

// matching table
static struct of_device_id esl_audio_of_ids[] = {
    {.compatible = "esl,audio-fifo"},
    {}};

// structure for class of all audio drivers
struct esl_audio_driver
{
  dev_t first_devno;
  struct class *class;
  unsigned int instance_count;    // how many drivers have been instantiated?
  struct list_head instance_list; // pointer to first instance
};

// allocate and initialize global data (class level)
static struct esl_audio_driver driver_data = {
    .first_devno = 0,
    .instance_count = 0,
    .instance_list = LIST_HEAD_INIT(driver_data.instance_list),
};

/* Utility Functions */

// find instance from inode using minor number and linked list
static struct esl_audio_instance *inode_to_instance(struct inode *i)
{
  struct esl_audio_instance *inst_iter;
  unsigned int minor = iminor(i);

  // start with fist element in linked list (stored at class),
  // and iterate through its elements
  list_for_each_entry(inst_iter, &driver_data.instance_list, inst_list)
  {
    // found matching minor number?
    if (MINOR(inst_iter->devno) == minor)
    {
      // return instance pointer of corresponding instance
      return inst_iter;
    }
  }

  // not found
  return NULL;
}

// return instance struct based on file
static struct esl_audio_instance *file_to_instance(struct file *f)
{
  return inode_to_instance(f->f_path.dentry->d_inode);
}

/* Character device File Ops */
static ssize_t esl_audio_write(struct file *f,
                               const char __user *buf, size_t len,
                               loff_t *offset)
{
  struct esl_audio_instance *inst = file_to_instance(f);
  unsigned int written = 0;
  unsigned int space;
  unsigned int to_write;
  int err, i;

  if (!inst)
  {
    // instance not found
    return -ENOENT;
  }

  while (len > 0)
  {
    space = ioread32(inst->regs + TDFV) * 4; // Each FIFO slot is 4 bytes

     wait_event_interruptible(inst->waitq, (inst->regs+ISR) & (1 << TX_FULL)); 

    if (space == 0)
    {
      // FIFO is full, wait
      usleep_range(1000, 2000); // check the range

     

      continue;
    }

    to_write = min((size_t)space, len);

    // Copy from user space to kernel buffer
    if (copy_from_user(inst->fifo_buf, buf + written, to_write))
    {
      return -EFAULT;
    }

    // Write to AXI FIFO
    for (i = 0; i < to_write; i += 4)
    {
      iowrite32(*(u32 *)(inst->fifo_buf + i), inst->regs + TDFD);
    }

    iowrite32(to_write, inst->regs + TLR);

    written += to_write;
    len -= to_write;
  }

  return written;
}

// definition of file operations
struct file_operations esl_audio_fops = {
    .write = esl_audio_write,
};

// define the class, creates /sys/class/audio
static struct class esl_audio_class = {
    .name = "audio",
    .owner = THIS_MODULE,
};

/* interrupt handler */
static irqreturn_t esl_audio_irq_handler(int irq, void *dev_id)
{
  struct esl_audio_instance *inst = dev_id;
  unsigned int ISR_reg;

  // Reads the interrupt status
  ISR_reg = ioread32(inst->regs + ISR);

  // Check for TX overrun interrupt
  if (ISR_reg & (1 << TX_OVERRUN_ERR))
  {
    printk(KERN_WARNING "TX overrun error\n");
    return -EINVAL;
  }

  // Check for TX size error interrupt
  else if (ISR_reg & (1 << TX_SIZE_ERR))
  {
    printk(KERN_WARNING "TX size error\n");
    return -EINVAL;
  }

  // Check for TX full interrupt
  else if (ISR_reg & (1 << TX_EMPTY))
  {
    // begsins to send the TX fifo
    wake_up();

  }

  // Clear the interrupt
  iowrite32(ISR_reg, inst->regs + ISR);

  return IRQ_HANDLED;
}

static int esl_audio_probe(struct platform_device *pdev)
{
  struct esl_audio_instance *inst = NULL;
  int err;
  struct resource *res;
  struct device *dev;
  const void *prop;
  phandle i2s_phandle;
  struct device_node *i2sctl_node;

  dev_t devno = MKDEV(driver_data.first_devno, driver_data.instance_count);

  // allocate instance
  inst = devm_kzalloc(&pdev->dev, sizeof(struct esl_audio_instance),
                      GFP_KERNEL);

  if (!inst)
  {
    // ran out of memory
    return -ENOMEM;
  }

  // set platform driver data
  platform_set_drvdata(pdev, inst);

  // get registers (AXI FIFO)
  res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if (IS_ERR(res))
  {
    return PTR_ERR(res);
  }

  inst->regs = devm_ioremap_resource(&pdev->dev, res);
  if (IS_ERR(inst->regs))
  {
    return PTR_ERR(inst->regs);
  }

  // get TX fifo depth
  err = of_property_read_u32(pdev->dev.of_node, "xlnx,tx-fifo-depth",
                             &inst->tx_fifo_depth);
  if (err)
  {
    printk(KERN_ERR "%s: failed to retrieve TX fifo depth\n",
           DRIVER_NAME);
    return err;
  }

  // get interrupt
  res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
  if (IS_ERR(res))
  {
    return PTR_ERR(res);
  }

  err = devm_request_irq(&pdev->dev, res->start,
                         esl_audio_irq_handler,
                         IRQF_TRIGGER_HIGH,
                         "zedaudio", inst);
  if (err < 0)
  {
    return err;
  }

  // save irq number
  inst->irqnum = res->start;

  // get and inspect i2s reference
  prop = of_get_property(pdev->dev.of_node, "esl,i2s-controller", NULL);
  if (IS_ERR(prop))
  {
    return PTR_ERR(prop);
  }

  // cast into phandle
  i2s_phandle = be32_to_cpu(*(phandle *)prop);
  i2sctl_node = of_find_node_by_phandle(i2s_phandle);
  if (!i2sctl_node)
  {
    // couldnt find
    printk(KERN_ERR "%s: could not find AXI I2S controller", DRIVER_NAME);
    return -ENODEV;
  }

  // get controller instance pointer from device node
  inst->i2s_controller = i2sctl_node->data;
  of_node_put(i2sctl_node);

  // create character device

  // get device number
  inst->devno = MKDEV(MAJOR(driver_data.first_devno),
                      driver_data.instance_count);

  printk(KERN_INFO "Device probed with devno #%u", inst->devno);
  printk(KERN_INFO "Device probed with inst count #%u", driver_data.instance_count);

  // initialize and create character device
  /* dev = device_create_with_groups(&esl_audio_class, &pdev->dev,
                                   devno, inst, esl_audio_attr_groups,
                                   "audio%d", driver_data.instance_number); */

  cdev_init(&inst->chr_dev, &esl_audio_fops);
  inst->chr_dev.owner = THIS_MODULE;
  err = cdev_add(&inst->chr_dev, inst->devno, 1);
  if (err)
  {
    printk(KERN_ERR "Failed to add cdev\n");
    return err;
  }

  printk(KERN_INFO "inst->devno minor %u", MINOR(inst->devno));

  // Device creation for sysfs
  dev = device_create(driver_data.class, &pdev->dev, inst->devno, NULL, "zedaudio%d", MINOR(inst->devno));
  if (IS_ERR(dev))
  {
    cdev_del(&inst->chr_dev);
    return PTR_ERR(dev);
  }

  printk(KERN_INFO "after device_create");

  // increment instance count
  driver_data.instance_count++;

  // put into list
  INIT_LIST_HEAD(&inst->inst_list);
  list_add(&inst->inst_list, &driver_data.instance_list);

  // init wait queue
  init_waitqueue_head(&inst->waitq);

  iowrite32(0x000000A5, inst->regs + TDFR);
  // Reset AXI FIFO
  //*(volatile unsigned int *)REG_OFFSET(inst->regs, TDFR) = 0x000000A5;

  // Enables interrupts

  iowrite32(0x0C000000, inst->regs + IER);
  //*(volatile unsigned int *)REG_OFFSET(inst->regs, IER) = 0x000000A5;

  // write_reg(audioRegs_global, IER, 0x0C000000);

  return 0;
}

static int esl_audio_remove(struct platform_device *pdev)
{
  struct esl_audio_instance *inst = platform_get_drvdata(pdev);

  // cleanup and remove
  device_destroy(&esl_audio_class, inst->devno);

  // remove from list
  list_del(&inst->inst_list);

  return 0;
}

// platform driver definition
static struct platform_driver esl_audio_driver = {
    .probe = esl_audio_probe,
    .remove = esl_audio_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = of_match_ptr(esl_audio_of_ids),
    },
};

static int esl_audio_init(void)
{
  int err;
  unsigned char *audioRegs;

  // alocate character device region
  err = alloc_chrdev_region(&driver_data.first_devno, 0, 16, "zedaudio");
  if (err < 0)
  {
    return err;
  }



  // although not using sysfs, still necessary in order to automatically
  // get device node in /dev
  driver_data.class = class_create(THIS_MODULE, "zedaudio");
  if (IS_ERR(driver_data.class))
  {
    return -ENOENT;
  }

  platform_driver_register(&esl_audio_driver);

  return 0;
}

static void esl_audio_exit(void)
{

  // free character device region
  unregister_chrdev_region(driver_data.first_devno, 16);

  // remove class
  class_destroy(driver_data.class);

  // plat driver unregister
  platform_driver_unregister(&esl_audio_driver);

}

module_init(esl_audio_init);
module_exit(esl_audio_exit);

MODULE_DESCRIPTION("ZedBoard Simple Audio driver");
MODULE_LICENSE("GPL");
