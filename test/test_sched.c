
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <semphr.h>
#include "unity.h"

#define configGENERATE_RUN_TIME_STATS 1

#define LOW_PRIO      (tskIDLE_PRIORITY + 1)
#define MED_PRIO      (tskIDLE_PRIORITY + 2)
#define HIGH_PRIO     (tskIDLE_PRIORITY + 3)
#define STACK_SIZE    configMINIMAL_STACK_SIZE

uint64_t t1, t2;

SemaphoreHandle_t shared_sem;
volatile bool high_ran = false;

void setUp() {}
void tearDown() {}

void low_task(void *arg)
{
    xSemaphoreTake(shared_sem, portMAX_DELAY);
    printf("Low: Took semaphore\n");
    vTaskDelay(pdMS_TO_TICKS(2000)); // hold the semaphore a while
    printf("Low: Releasing semaphore\n");
    xSemaphoreGive(shared_sem);
    vTaskDelete(NULL);
}

void medium_task(void *arg)
{
    // Keeps the CPU busy while high waits
    for (;;) {
        // printf("Medium: Running\n");
        vTaskDelay(pdMS_TO_TICKS(100)); // simulate doing periodic work
    }
}

void high_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(50)); // let low start first
    printf("High: Trying to take semaphore...\n");
    if (xSemaphoreTake(shared_sem, pdMS_TO_TICKS(1000)) == pdTRUE) {
        printf("High: Got semaphore!\n");
        high_ran = true;
        xSemaphoreGive(shared_sem);
    } else {
        printf("High: Timed out waiting for semaphore\n");
    }
    vTaskDelete(NULL);
}

void busy_busy(void *args)
{
    for (int i = 0; ; i++);
}

void busy_yield(void *args)
{
    for (int i = 0; ; i++) {
        taskYIELD();
    }
}

// Activity 0
void test_priority_inversion(void)
{
    shared_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(shared_sem); // make it available initially

    xTaskCreate(low_task, "Low", STACK_SIZE, NULL, LOW_PRIO, NULL);
    xTaskCreate(medium_task, "Med", STACK_SIZE, NULL, MED_PRIO, NULL);
    xTaskCreate(high_task, "High", STACK_SIZE, NULL, HIGH_PRIO, NULL);

    // Let the scenario play out
    vTaskDelay(pdMS_TO_TICKS(4000));

    TEST_ASSERT_EQUAL(false, high_ran); // should not have run due to inversion

    vSemaphoreDelete(shared_sem);
}

// Activity 1
void test_priority_inversion_mutex()
{
    shared_sem = xSemaphoreCreateMutex();

    xTaskCreate(low_task, "Low", STACK_SIZE, NULL, LOW_PRIO, NULL);
    xTaskCreate(medium_task, "Med", STACK_SIZE, NULL, MED_PRIO, NULL);
    xTaskCreate(high_task, "High", STACK_SIZE, NULL, HIGH_PRIO, NULL);

    // Let the scenario play out
    vTaskDelay(pdMS_TO_TICKS(4000));

    TEST_ASSERT_EQUAL(false, high_ran); // should not have run due to inversion

    vSemaphoreDelete(shared_sem);
}

// Activity 2
void both_busy_busy()
{
    TaskHandle_t first_task;
    TaskHandle_t second_task;

    t1 = 0;
    t2 = 0;
    xTaskCreate(busy_busy, "first", STACK_SIZE, NULL, MED_PRIO, &first_task);
    xTaskCreate(busy_busy, "second", STACK_SIZE, NULL, MED_PRIO, &second_task);

    vTaskDelay(pdMS_TO_TICKS(4000));

    t1 = ulTaskGetRunTimeCounter(first_task);
    t2 = ulTaskGetRunTimeCounter(second_task);

    printf("first runtime: %llu\n", t1);
    printf("second runtime: %llu\n", t2);

    // Expected Behavior: approx equal time ran

    vTaskDelete(first_task);
    vTaskDelete(second_task);


}
void both_busy_yield()
{
    TaskHandle_t first_task;
    TaskHandle_t second_task;
    t1 = 0;
    t2 = 0;
    
    xTaskCreate(busy_yield, "first", STACK_SIZE, NULL, MED_PRIO, &first_task);
    xTaskCreate(busy_yield, "second", STACK_SIZE, NULL, MED_PRIO, &second_task);

    vTaskDelay(pdMS_TO_TICKS(4000));
    
    t1 = ulTaskGetRunTimeCounter(first_task);
    t2 = ulTaskGetRunTimeCounter(second_task);

    printf("first runtime: %llu\n", t1);
    printf("second runtime: %llu\n", t2);
    
    // Expected Behavior: approx equal time ran

    vTaskDelete(first_task);
    vTaskDelete(second_task);

}
void thread1_busy_thread2_yield()
{
    TaskHandle_t first_task;
    TaskHandle_t second_task;
    t1 = 0;
    t2 = 0;

    xTaskCreate(busy_busy, "first", STACK_SIZE, NULL, MED_PRIO, &first_task);
    xTaskCreate(busy_yield, "second", STACK_SIZE, NULL, MED_PRIO, &second_task);

    vTaskDelay(pdMS_TO_TICKS(4000));

    t1 = ulTaskGetRunTimeCounter(first_task);
    t2 = ulTaskGetRunTimeCounter(second_task);

    printf("first runtime: %llu\n", t1);
    printf("second runtime: %llu\n", t2);

    // Expected Behavior: busy_busy hogs processor time
    
    vTaskDelete(first_task);
    vTaskDelete(second_task);    
}

void diff_prior_both_busy_high1()
{
    // Expected Behavior: high priority hogs processor time (low priority does not get scheduled)
}

void diff_prior_both_busy_low1()
{
    // Expected behavior: high priority hogs processor time
}
void diff_prior_both_yield()
{
    // Expected behavior: high priority hogs processor time (low priority does not run)    
}

void runner_thread(__unused void *args)
{
    for (;;) {
        printf("Starting test run\n");
        UNITY_BEGIN();
        RUN_TEST(test_priority_inversion);
        RUN_TEST(test_priority_inversion_mutex);
        RUN_TEST(both_busy_busy);
        RUN_TEST(both_busy_yield);
        RUN_TEST(thread1_busy_thread2_yield);
        // RUN_TEST(diff_prior_both_busy_high1);
        // RUN_TEST(diff_prior_both_busy_low1);
        // RUN_TEST(diff_prior_both_yield);
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
                STACK_SIZE, NULL, HIGH_PRIO + 1, NULL);
    vTaskStartScheduler();
    return 0;
}
