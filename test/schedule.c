#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <stdint.h>
#include <semphr.h>
#include "unity_config.h"
#include <unity.h>

#define TEST_TASK_PRIORITY      ( tskIDLE_PRIORITY + 1UL )
#define TEST_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define TEST_RUNNER_PRIORITY      ( tskIDLE_PRIORITY + 10UL )
#define TEST_RUNNER_STACK_SIZE configMINIMAL_STACK_SIZE

#define THREAD_COUNT 3

struct task_args
{
    SemaphoreHandle_t lock;

};

TaskHandle_t supervisor_thread;
TaskHandle_t worker_threads[THREAD_COUNT];

struct task_args worker_args[THREAD_COUNT];

SemaphoreHandle_t semaphore_mutex;

int setup_semaphore;
int setup_mutex;

void handler_task(void *vargs)
{
    struct task_args *args = (struct task_args *) vargs;
    xSemaphoreTake(args->lock, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(500));
    // xSemaphoreGive(args->lock);

}

void setUp(void) 
{
    if (setup_semaphore)
    {
        semaphore_mutex = xSemaphoreCreateBinary();
        xSemaphoreGive(semaphore_mutex); // inits to taken, make available
    }

    if (setup_mutex)
    {
        semaphore_mutex = xSemaphoreCreateMutex();
    }

    for (int t = 0; t < THREAD_COUNT; t++) 
    {
        worker_args[t].lock = semaphore_mutex;
        xTaskCreate(handler_task,
                "worker",
                TEST_TASK_STACK_SIZE,
                (void *)(worker_args + t),
                TEST_TASK_PRIORITY + t,
                worker_threads + t);
        busy_wait_us(10000);
    }
}

void tearDown(void)
{
    if(setup_semaphore || setup_mutex)
    {
        vSemaphoreDelete(semaphore_mutex); // delete semaphore/mutex
    }

    for (int t = 0; t < THREAD_COUNT; t++) 
    {
        vTaskDelete(worker_threads[t]); // delete tasks
    }
}

void test_priority_inversion(void)
{
    int ret = xSemaphoreTake(semaphore_mutex, 100);
    TEST_ASSERT_EQUAL_INT(pdFALSE, ret);
}

void test_priority_inversion_mutex(void)
{
    int ret = xSemaphoreTake(semaphore_mutex, 100);
    TEST_ASSERT_EQUAL_INT(pdFALSE, ret);
}

void runner_thread (__unused void *args)
{
    for (;;) {
        printf("Starting test run\n");
        UNITY_BEGIN();

        setup_semaphore = 1;
        setup_mutex = 0;
        RUN_TEST(test_priority_inversion);
        
        setup_semaphore = 0;
        setup_mutex = 1;
        RUN_TEST(test_priority_inversion_mutex);
        
        setup_semaphore = 0;
        setup_mutex = 0;
        UNITY_END();
        sleep_ms(10000);
    }
}

int main(void)
{
    stdio_init_all();
    hard_assert(cyw43_arch_init() == PICO_OK);
    printf("Launching runner\n");
    xTaskCreate(runner_thread, "TestRunner",
                TEST_RUNNER_STACK_SIZE, NULL,
                TEST_RUNNER_PRIORITY, &supervisor_thread);
    vTaskStartScheduler();
	return 0;
}
