/**
 * @file mpu6050_trigger.c
 * @brief Interrupt-driven sensor trigger support for the MPU6050.
 *
 * @details Implements the data-ready trigger backed by the sensor's INT
 * pin: GPIO interrupt setup, the ISR-context callback, and dispatch of the
 * registered handler on either a dedicated thread or the system workqueue,
 * depending on @kconfig{CONFIG_TMI_DRIVER_MPU6050_TRIGGER_OWN_THREAD} vs.
 * @kconfig{CONFIG_TMI_DRIVER_MPU6050_TRIGGER_GLOBAL_THREAD}. Compiled in only
 * when @kconfig{CONFIG_TMI_DRIVER_MPU6050_TRIGGER} is enabled.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include "mpu6050.h"

LOG_MODULE_DECLARE(mpu6050);

/**
 * @brief Configure the data-ready trigger.
 *
 * @details
 * Only @ref SENSOR_TRIG_DATA_READY is supported. Passing a NULL @p handler
 * disables the trigger and its GPIO interrupt.
 *
 * @param[in] dev MPU6050 device instance. Must not be NULL.
 * @param[in] trig Trigger configuration. Must not be NULL.
 * @param[in] handler Callback invoked when the trigger fires, or NULL to
 * disable the trigger.
 *
 * @retval 0 Trigger was configured successfully.
 * @retval -EINVAL If @p dev or @p trig is NULL.
 * @retval -ENOTSUP If the device has no configured interrupt GPIO, or @p trig
 * is not @ref SENSOR_TRIG_DATA_READY.
 * @return Negative errno from GPIO interrupt configuration on failure.
 */
int mpu6050_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
			sensor_trigger_handler_t handler)
{
	CHECK_NULL_PTR(dev);
	CHECK_NULL_PTR(trig);

	const mpu6050_config_t *cfg = (const mpu6050_config_t *)dev->config;
	mpu6050_data_t *data = (mpu6050_data_t *)dev->data;

	if (cfg->int_gpio.port == NULL) {
		return -ENOTSUP;
	}

	if (trig->type != SENSOR_TRIG_DATA_READY) {
		return -ENOTSUP;
	}

	int ret = gpio_pin_interrupt_configure_dt(&cfg->int_gpio, GPIO_INT_DISABLE);
	if (ret != 0) {
		LOG_ERR("Failed to disable GPIO interrupt: %d", ret);
		return ret;
	}

	data->data_ready_handler = handler;
	if (handler == NULL) {
		return 0;
	}

	data->data_ready_trigger = trig;

	ret = gpio_pin_interrupt_configure_dt(&cfg->int_gpio, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_ERR("Failed to enable GPIO interrupt: %d", ret);
		return ret;
	}

	return 0;
}

static void mpu6050_handle_data_ready(const struct device *dev)
{
	const mpu6050_config_t *cfg = (const mpu6050_config_t *)dev->config;
	mpu6050_data_t *data = (mpu6050_data_t *)dev->data;

	if (data->data_ready_handler != NULL) {
		data->data_ready_handler(dev, data->data_ready_trigger);
	}

	int ret = gpio_pin_interrupt_configure_dt(&cfg->int_gpio, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_ERR("Failed to re-enable GPIO interrupt: %d", ret);
	}
}

static void mpu6050_gpio_callback(const struct device *port, struct gpio_callback *cb,
				  uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(pins);

	mpu6050_data_t *data = CONTAINER_OF(cb, mpu6050_data_t, gpio_cb);
	const mpu6050_config_t *cfg = (const mpu6050_config_t *)data->dev->config;

	int ret = gpio_pin_interrupt_configure_dt(&cfg->int_gpio, GPIO_INT_DISABLE);
	if (ret != 0) {
		LOG_ERR("Failed to disable GPIO interrupt: %d", ret);
	}

#if defined(CONFIG_TMI_DRIVER_MPU6050_TRIGGER_OWN_THREAD)
	k_sem_give(&data->gpio_sem);
#elif defined(CONFIG_TMI_DRIVER_MPU6050_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&data->work);
#endif
}

#if defined(CONFIG_TMI_DRIVER_MPU6050_TRIGGER_OWN_THREAD)
static void mpu6050_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	mpu6050_data_t *data = (mpu6050_data_t *)p1;

	while (1) {
		k_sem_take(&data->gpio_sem, K_FOREVER);
		mpu6050_handle_data_ready(data->dev);
	}
}
#elif defined(CONFIG_TMI_DRIVER_MPU6050_TRIGGER_GLOBAL_THREAD)
static void mpu6050_work_cb(struct k_work *work)
{
	mpu6050_data_t *data = CONTAINER_OF(work, mpu6050_data_t, work);

	mpu6050_handle_data_ready(data->dev);
}
#endif

/**
 * @brief Initialize the interrupt GPIO and enable the data-ready interrupt.
 *
 * @param[in] dev MPU6050 device instance. Must not be NULL.
 *
 * @retval 0 Interrupt was initialized successfully.
 * @retval -EINVAL If @p dev is NULL.
 * @retval -ENODEV If the interrupt GPIO device is not ready.
 * @retval -EIO If the GPIO callback could not be registered.
 * @return Negative errno from GPIO or register configuration on failure.
 */
int mpu6050_init_interrupt(const struct device *dev)
{
	CHECK_NULL_PTR(dev);

	const mpu6050_config_t *cfg = (const mpu6050_config_t *)dev->config;
	mpu6050_data_t *data = (mpu6050_data_t *)dev->data;

	if (!gpio_is_ready_dt(&cfg->int_gpio)) {
		LOG_ERR("Interrupt GPIO device not ready");
		return -ENODEV;
	}

	data->dev = dev;

	int ret = gpio_pin_configure_dt(&cfg->int_gpio, GPIO_INPUT);
	if (ret != 0) {
		LOG_ERR("Failed to configure interrupt GPIO: %d", ret);
		return ret;
	}

	gpio_init_callback(&data->gpio_cb, mpu6050_gpio_callback, BIT(cfg->int_gpio.pin));

	ret = gpio_add_callback(cfg->int_gpio.port, &data->gpio_cb);
	if (ret != 0) {
		LOG_ERR("Failed to set GPIO callback: %d", ret);
		return -EIO;
	}

	/* Enable the data-ready interrupt. Push-pull, active-high, 50us pulse
	 * (INT_PIN_CFG power-on-reset default) is used, so INT_STATUS does
	 * not need to be read to clear it. */
	ret = mpu6050_write_reg(dev, MPU6050_REG_INT_ENABLE, MPU6050_MASK_INT_ENABLE_DATA_RDY_EN);
	if (ret != 0) {
		LOG_ERR("Failed to enable data ready interrupt: %d", ret);
		return ret;
	}

#if defined(CONFIG_TMI_DRIVER_MPU6050_TRIGGER_OWN_THREAD)
	k_sem_init(&data->gpio_sem, 0, K_SEM_MAX_LIMIT);

	k_thread_create(&data->thread, data->thread_stack,
			CONFIG_TMI_DRIVER_MPU6050_THREAD_STACK_SIZE, mpu6050_thread, data, NULL,
			NULL, K_PRIO_COOP(CONFIG_TMI_DRIVER_MPU6050_THREAD_PRIORITY), 0, K_NO_WAIT);
#elif defined(CONFIG_TMI_DRIVER_MPU6050_TRIGGER_GLOBAL_THREAD)
	data->work.handler = mpu6050_work_cb;
#endif

	ret = gpio_pin_interrupt_configure_dt(&cfg->int_gpio, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		LOG_ERR("Failed to enable GPIO interrupt: %d", ret);
		return ret;
	}

	return 0;
}
