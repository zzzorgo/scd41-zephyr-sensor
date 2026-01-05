#include <zephyr/kernel.h>

int main(void)
{
#ifdef CONFIG_SAMPLE_DO_OUTPUT
	printk("Hello World from minimal!\n");
#endif

	return 0;
}
