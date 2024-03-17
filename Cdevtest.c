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

#define DRIVER_NAME "esl-audio"

// forward declaration of axi_i2s
struct axi_i2s;

// declare functions we will use from the esli2s driver
extern void esl_codec_enable_tx(struct axi_i2s* i2s);
extern void esl_codec_disable_tx(struct axi_i2s* i2s);

// structure for an individual instance (ie. one audio driver)
struct esl_audio_instance
{
  void* __iomem regs; //fifo registers
  struct cdev chr_dev; //character device
  dev_t devno;

  // list head
  struct list_head inst_list;

  // interrupt number
  unsigned int irqnum;

  // fifo depth
  unsigned int tx_fifo_depth;

  // wait queue
  wait_queue_head_t waitq;

  // i2s controller instance
  struct axi_i2s* i2s_controller;
};

// structure for class of all audio drivers
struct esl_audio_driver
{
  dev_t first_devno;
  struct class* class;
  unsigned int instance_count;     // how many drivers have been instantiated?
  struct list_head instance_list;  // pointer to first instance
};

// allocate and initialize global data (class level)
static struct esl_audio_driver driver_data = {
  .instance_count = 0,
  .instance_list = LIST_HEAD_INIT(driver_data.instance_list),
};

/* Utility Functions */

// find instance from inode using minor number and linked list
static struct esl_audio_instance* inode_to_instance(struct inode* i)
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
static struct esl_audio_instance* file_to_instance(struct file* f)
{
  return inode_to_instance(f->f_path.dentry->d_inode);
}

/* Character device File Ops */
static ssize_t esl_audio_write(struct file* f,
                               const char __user *buf, size_t len,
                               loff_t* offset)
{
  struct esl_audio_instance *inst = file_to_instance(f);
  unsigned int written = 0;

  if (!inst)
    {
      // instance not found
      return -ENOENT;
    }

  // TODO Implement write to AXI FIFO

  return written;
}

// definition of file operations 
struct file_operations esl_audio_fops = {
  .write = esl_audio_write,
};

/* interrupt handler */
static irqreturn_t esl_audio_irq_handler(int irq, void* dev_id)
{
  struct esl_audio_instance* inst = dev_id;

  // TODO handle interrupts

  return IRQ_HANDLED;
}

static int esl_audio_probe(struct platform_device* pdev)
{
  struct esl_audio_instance* inst = NULL;
  int err;
  struct resource* res;
  const void* prop;
  phandle i2s_phandle;
  struct device_node* i2sctl_node;

  // allocate instance
  inst = devm_kzalloc(&pdev->dev, sizeof(struct esl_audio_instance),
                      GFP_KERNEL);

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
  i2s_phandle = be32_to_cpu(*(phandle*)prop);
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

  // TODO initialize and create character device

  // increment instance count
  driver_data.instance_count++;

  // put into list
  INIT_LIST_HEAD(&inst->inst_list);
  list_add(&inst->inst_list, &driver_data.instance_list);

  // init wait queue
  init_waitqueue_head(&inst->waitq);

  // TODO reset AXI FIFO

  // TODO enable interrupts

  return 0;
}

static int esl_audio_remove(struct platform_device* pdev)
{
  struct esl_audio_instance* inst = platform_get_drvdata(pdev);

  // TODO remove all traces of character device

  // remove from list
  list_del(&inst->inst_list);

  return 0;
}

// matching table
static struct of_device_id esl_audio_of_ids[] = {
  { .compatible = "esl,audio-fifo" },
  { }
};

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

  // alocate character device region
  err = alloc_chrdev_region(&driver_data.first_devno, 0, 16, "zedaudio");
  if (err < 0)
    {
      return err;
    }

  // create class
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
  platform_driver_unregister(&esl_audio_driver);

  // free character device region
  unregister_chrdev_region(driver_data.first_devno, 16);

  // remove class
  class_destroy(driver_data.class);
}

module_init(esl_audio_init);
module_exit(esl_audio_exit);

MODULE_DESCRIPTION("ZedBoard Simple Audio driver");
MODULE_LICENSE("GPL");
