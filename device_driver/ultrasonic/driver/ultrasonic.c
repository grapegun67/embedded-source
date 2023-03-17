#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#define GPIO_HIGH	1
#define GPIO_LOW	0

#define ECHO_VALID	1
#define ECHO_UNVALID	0

#define HC_SR04_ECHO_PIN	5
#define HC_SR04_TRIG_PIN	6

static int gpio_irq = -1;
static int echo_valid_flag = 0;
static ktime_t echo_start;
static ktime_t echo_stop;

static int sonic_open(struct inode *inode, struct file *file)
{
        pr_info("device open\n");
        return 0;
}

static int sonic_release(struct inode *inode, struct file *file)
{
        pr_info("device closed\n");
        return 0;
}

/* /sys/class/hc_sr04/value 파일의 내용을 읽어 들임 - TRIG pin을 설정하여 초음파 소리 전달 시작하도록 만듦	*/
static ssize_t hc_sr04_value_show(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	int counter;

	gpio_set_value(HC_SR04_TRIG_PIN, GPIO_HIGH);

	/* datasheet에 의하면 최소 10 us 동안 Trig pin이 HIGH 상태로 유지되어야 함 */
	udelay(15);   

	gpio_set_value(HC_SR04_TRIG_PIN, GPIO_LOW);
	echo_valid_flag = ECHO_UNVALID;

	counter = 0;
	while (echo_valid_flag == ECHO_UNVALID)
	{
		if (++counter > 2320000)
		{
			pr_info(KERN_INFO "debug0 [%d]\n", counter);
			return sprintf(buf, "[%d] [%d]\n", counter,  -1);/* copy_to_user() 사용해야할지도	*/
		}

		udelay(1);
	}

	/* 초음파를 전송한 후, 벽에 부딛혀 돌아오는데 걸린 시간 - microsecond */
	return sprintf(buf, "%lld\n", ktime_to_us(ktime_sub(echo_stop, echo_start)));  
}

/* /sys/class/hc_sr04/value 파일에 값을 씀 - 실제로는 하는 일 없음	*/
static ssize_t hc_sr04_value_store(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	pr_info(KERN_INFO "Buffer len %d bytes\n", len);
	return len;
}

dev_t	dev = 0;
static struct cdev sonic_cdev;
static struct class *dev_class;

static struct file_operations fops =
{
        .owner          = THIS_MODULE,
        .read           = hc_sr04_value_show,
        .write          = hc_sr04_value_store,
        .open           = sonic_open,
        .release        = sonic_release,
};

/* echo pin 관련 interrupt handler 함수 - ECHO pin이 HIGH일 때의 시간과 LOW일 때의 시간을 구함, 둘의 차이 값을 거리 계산시 사용함 */
static irqreturn_t echo_interrupt_handler(int irq, void *data)
{
	ktime_t time_value;

	pr_info(KERN_INFO "debug1");

	if (echo_valid_flag == ECHO_UNVALID)
	{
		time_value = ktime_get();

		pr_info(KERN_INFO "debug2");
		if (gpio_get_value(HC_SR04_ECHO_PIN) == GPIO_HIGH) 
			echo_start = time_value;
		else
		{
			echo_stop = time_value;
			echo_valid_flag = ECHO_VALID;
		}
    
	}

	return IRQ_HANDLED;
}

static int __init hc_sr04_init(void)
{
  int ret;
  
  if((alloc_chrdev_region(&dev, 0, 1, "sonic_dev")) < 0)
	{
    pr_err("Error: cannot allocate major number\n");
		goto error;
  }
  
  pr_info("Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));

  cdev_init(&sonic_cdev, &fops);

  if((cdev_add(&sonic_cdev, dev, 1)) < 0)
	{
    pr_err("Error: annot add device \n");
		goto error;
  }

  if((dev_class = class_create(THIS_MODULE, "sonic_class")) == NULL)
	{
    pr_err("Error: cannot create the struct class\n");
		goto error;
  }

  if((device_create(dev_class, NULL, dev, NULL, "sonic_device")) == NULL)
	{
    pr_err( "Error: cannot create device \n");
		goto error;
  }

	ret = gpio_request_one(HC_SR04_TRIG_PIN, GPIOF_DIR_OUT, "TRIG");  /* TRIG pin GPIO output 사용 요청 */
	if (ret < 0)
	{
		pr_info(KERN_ERR "Failed to request gpio for TRIG(%d)\n", ret);
		return -1;
	}

	ret = gpio_request_one(HC_SR04_ECHO_PIN, GPIOF_DIR_IN, "ECHO");  /* ECHO pin GPIO input 사용 요청 */
	if (ret < 0)
	{
		pr_info(KERN_ERR "Failed to request gpio for ECHO(%d)\n", ret);
		return -1;
	}

	ret = gpio_to_irq(HC_SR04_ECHO_PIN);  /* ECHO pin을 interrupt 가능하도록 요청 */
	if (ret < 0)
	{
		pr_info(KERN_ERR "Failed to set gpio IRQ(%d)\n", ret);
		goto error;
	}

	else
    gpio_irq = ret;
		                  		    /* interrupt handler 등록 */  	/* interrupt 발생 조건 */
	ret = request_irq(gpio_irq, echo_interrupt_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "hc-sr04-echo", NULL);
	pr_info(KERN_ERR "request IRQ(%d)\n", ret);
	if (ret)
	{
		pr_info(KERN_ERR "Failed to request IRQ(%d)\n", ret);
		goto error;
	}

  return 0;

error:
    return -1;
}

static void __exit hc_sr04_exit(void)
{
	/* interrupt 등록 해제 */
	if (gpio_irq != -1)
    free_irq(gpio_irq, NULL);  

 	gpio_unexport(HC_SR04_TRIG_PIN);
  gpio_unexport(HC_SR04_ECHO_PIN);

	gpio_free(HC_SR04_TRIG_PIN);	/* GPIO 설정 해제 */
	gpio_free(HC_SR04_ECHO_PIN);	/* GPIO 설정 해제 */
  device_destroy(dev_class,dev);
  class_destroy(dev_class);
  cdev_del(&sonic_cdev);
  unregister_chrdev_region(dev, 1);

  pr_info("GPIO driver done\n");
}

module_init(hc_sr04_init);
module_exit(hc_sr04_exit);

MODULE_AUTHOR("gun");
MODULE_DESCRIPTION("hc-sr04 ultrasonic distance sensor driver");
MODULE_LICENSE("GPL");
