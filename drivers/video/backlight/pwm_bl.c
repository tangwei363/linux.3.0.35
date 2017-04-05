/*
 * linux/drivers/video/backlight/pwm_bl.c
 *
 * simple PWM based backlight control, board code has to setup
 * 1) pin configuration so PWM waveforms can output
 * 2) platform_data being correctly configured
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/slab.h>

struct pwm_bl_data {
	unsigned int            pwm_id;
	struct pwm_device	*pwm[2];
	struct device		*dev;
	unsigned int		period;
	unsigned int		lth_brightness;
	int			(*notify)(struct device *,
					  int brightness);
	int			(*check_fb)(struct device *, struct fb_info *);
};

static struct backlight_device s_bl_dev;
static int s_bl_en = 1;
static int pwm_backlight_update_status(struct backlight_device *bl)
{
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);
	int brightness = bl->props.brightness;
	int max = bl->props.max_brightness;

	memcpy(&s_bl_dev, bl, sizeof(struct backlight_device));

	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if (pb->notify)
		brightness = pb->notify(pb->dev, brightness);

	if (brightness == 0) {
		if(pb->pwm_id==4){
			pwm_config(pb->pwm[0], 0, pb->period);
			pwm_disable(pb->pwm[0]);
			pwm_config(pb->pwm[1], 0, pb->period);
			pwm_disable(pb->pwm[1]);
        	}else{
			pwm_config(pb->pwm[0], 0, pb->period);
			pwm_disable(pb->pwm[0]);
		}
	} else {
	    printk("jzw;%s:s_bl_en=%d\n", __func__, s_bl_en);
	    if (s_bl_en == 0) {
            return;
	    }
		brightness = pb->lth_brightness +
			(brightness * (pb->period - pb->lth_brightness) / max);
		if(pb->pwm_id==4){
			pwm_config(pb->pwm[0], brightness, pb->period);
			pwm_enable(pb->pwm[0]);
			pwm_config(pb->pwm[1], brightness, pb->period);
			pwm_enable(pb->pwm[1]);
                }else{
			pwm_config(pb->pwm[0], brightness, pb->period);
			pwm_enable(pb->pwm[0]);
                }
	}
	return 0;
}

int pwm_backlight_enable(int enable)
{
    s_bl_en = enable;
}

int pwm_backlight_update(void)
{
    pwm_backlight_update_status(&s_bl_dev);    
}

static int pwm_backlight_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static int pwm_backlight_check_fb(struct backlight_device *bl,
					struct fb_info *info)
{
	char *id = info->fix.id;
	if (!strcmp(id, "DISP3 BG") ||
		!strcmp(id, "DISP3 BG - DI1") ||
		!strcmp(id, "DISP4 BG") ||
		!strcmp(id, "DISP4 BG - DI1"))
	    return 1;
	else
	return 0;
}

static const struct backlight_ops pwm_backlight_ops = {
	.update_status	= pwm_backlight_update_status,
	.get_brightness	= pwm_backlight_get_brightness,
	.check_fb	= pwm_backlight_check_fb,
};

static int pwm_backlight_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl;
	struct pwm_bl_data *pb;
	int ret;

	if (!data) {
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}

	if (data->init) {
		ret = data->init(&pdev->dev);
		if (ret < 0)
			return ret;
	}

	pb = kzalloc(sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	pb->period = data->pwm_period_ns;
	pb->notify = data->notify;
	pb->check_fb = data->check_fb;
	pb->lth_brightness = data->lth_brightness *
		(data->pwm_period_ns / data->max_brightness);
	pb->dev = &pdev->dev;
	pb->pwm_id=data->pwm_id;
	printk("pwm_id=%d\n\n\n",pb->pwm_id);
	if(pb->pwm_id==4){	
		pb->pwm[0] = pwm_request(0, "backlight0");
		pb->pwm[1] = pwm_request(2, "backlight2");
	}else if(pb->pwm_id==3){
		pb->pwm[0] = pwm_request(2, "backlight2");
	}else 
		pb->pwm[0] = pwm_request(0, "backlight0");
	if (IS_ERR(pb->pwm[0])) {
		dev_err(&pdev->dev, "unable to request PWM for backlight\n");
		ret = PTR_ERR(pb->pwm[0]);
		goto err_pwm;
	} else
		dev_dbg(&pdev->dev, "got pwm for backlight\n");

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = data->max_brightness;
	bl = backlight_device_register(dev_name(&pdev->dev), &pdev->dev, pb,
				       &pwm_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

	bl->props.brightness = data->dft_brightness;
	backlight_update_status(bl);

	platform_set_drvdata(pdev, bl);
    memcpy(&s_bl_dev, bl, sizeof(struct backlight_device));
	return 0;

err_bl:
	if(pb->pwm_id==4){
		pwm_free(pb->pwm[0]);
		pwm_free(pb->pwm[1]);
	}else
		pwm_free(pb->pwm[0]);
err_pwm:
	kfree(pb);
err_alloc:
	if (data->exit)
		data->exit(&pdev->dev);
	return ret;
}

static int pwm_backlight_remove(struct platform_device *pdev)
{
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);
	if(pb->pwm_id==4){
                pwm_config(pb->pwm[0], 0, pb->period);
                pwm_disable(pb->pwm[0]);
                pwm_config(pb->pwm[1], 0, pb->period);
                pwm_disable(pb->pwm[1]);
		pwm_free(pb->pwm[0]);
		pwm_free(pb->pwm[1]);
        }else {
                pwm_config(pb->pwm[0], 0, pb->period);
                pwm_disable(pb->pwm[0]);
		pwm_free(pb->pwm[0]);
        }
	kfree(pb);
	if (data->exit)
		data->exit(&pdev->dev);
	return 0;
}

#ifdef CONFIG_PM
static int pwm_backlight_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	if (pb->notify)
		pb->notify(pb->dev, 0);
	if(pb->pwm_id==4){
		pwm_config(pb->pwm[0], 0, pb->period);
		pwm_disable(pb->pwm[0]);
		pwm_config(pb->pwm[1], 0, pb->period);
		pwm_disable(pb->pwm[1]);
	}else {
		pwm_config(pb->pwm[0], 0, pb->period);
		pwm_disable(pb->pwm[0]);
	}
	return 0;
}

static int pwm_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);

	//backlight_update_status(bl);
	return 0;
}
#else
#define pwm_backlight_suspend	NULL
#define pwm_backlight_resume	NULL
#endif

void  pwm_backlight_shutdown(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);
	if(pb->pwm_id==4){
                pwm_config(pb->pwm[0], 0, pb->period);
                pwm_disable(pb->pwm[0]);
                pwm_config(pb->pwm[1], 0, pb->period);
                pwm_disable(pb->pwm[1]);
        }else {
                pwm_config(pb->pwm[0], 0, pb->period);
                pwm_disable(pb->pwm[0]);
        }
}

static struct platform_driver pwm_backlight_driver = {
	.driver		= {
		.name	= "pwm-backlight",
		.owner	= THIS_MODULE,
	},
	.probe		= pwm_backlight_probe,
	.remove		= pwm_backlight_remove,
	.suspend	= pwm_backlight_suspend,
	.resume		= pwm_backlight_resume,
	.shutdown	= pwm_backlight_shutdown,
};

static int __init pwm_backlight_init(void)
{
	return platform_driver_register(&pwm_backlight_driver);
}
//module_init(pwm_backlight_init);
late_initcall(pwm_backlight_init);

static void __exit pwm_backlight_exit(void)
{
	platform_driver_unregister(&pwm_backlight_driver);
}
module_exit(pwm_backlight_exit);

MODULE_DESCRIPTION("PWM based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pwm-backlight");

