#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <random>
#include <unistd.h>
#include <atomic>

sem_t reader_count_sem; // семафор для счетчика читателей
sem_t resource_sem; // семафор для ресурса

std::atomic<int> reader_count(0); // количество читателей (std::atomic для корректного чтения в главном потоке)
int size; // размер ресурса
int resource[100]; // ресурс (массив целых чисел)

/**
 * Возвращает рандомное число, лежащее в [min; max]
 * @param min Нижняя граница генерации
 * @param max Верхняя граница генерации
 * @return Рандомное число [min; max]
 */
int randomInt(int min, int max) {
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist6(min, max);

    return dist6(rng);
}

/**
 * Компаратор для сортировки массива
 * @param first Первый элемент
 * @param second Второй элемент
 * @return Результат сравнения: -1, 0 или 1
 */
int compare(const void *first, const void *second) {
    return (*(int *)first - *(int *)second);
}

/**
 * Функция потока-читателя
 * @param args
 * @return
 */
void *reader(void *args) {
    sem_wait(&reader_count_sem);
    ++reader_count;
    int current_reader = reader_count;

    if (reader_count == 1) {
        sem_wait(&resource_sem); // если это первый читатель, то блокируем доступ к ресурсу
    }
    sem_post(&reader_count_sem);

    int random_index = randomInt(0, size - 1);
    printf("\nReader %d: resource[%d] = %d\n", current_reader, random_index, resource[random_index]);

    sem_wait(&reader_count_sem);
    --reader_count;
    if (reader_count == 0) {
        sem_post(&resource_sem); // если это был последний читатель, то освобождаем доступ к ресурсу
    }
    sem_post(&reader_count_sem);
    return nullptr;
}

/**
 * Функция потока-писателя
 * @param args
 * @return
 */
void *writer(void *args) {
    sem_wait(&resource_sem); // поток-писатель всегда получает экслюзивный доступ к ресурсу
    printf("\nWriter: got exclusive access to the resource\n");
    resource[size] = randomInt(11, 111);
    usleep(randomInt(1000, 500000)); // имитируем долгую работу писателя
    printf("Writer: adding new element %d and sorting the array...\n", resource[size]);
    ++size;
    qsort(resource, size, sizeof(int), compare); // переводим ресурс в непротиворечивое состояние
    sem_post(&resource_sem); // освобождаем доступ к ресурсу
    printf("Writer: got released access to the resource\n");
    return nullptr;
}

/**
 * Входная точка в программу
 * @return
 */
int main() {
    size = 10; // изначальный размер массива
    for (int i = 0; i < size; ++i) {
        resource[i] = i;
    }

    pthread_t readers[10], writers[10]; // массив для потоков читателей и писателей
    sem_init(&reader_count_sem, 0, 1); //
    sem_init(&resource_sem, 0, 1); // инициализируем семафоры

    for (int i = 0; i < 10; i++) {
        pthread_create(&readers[i], nullptr, reader, nullptr);
        pthread_create(&writers[i], nullptr, writer, nullptr);

        /**
         * Чем больше количество потоков-читателей в данный момент, тем более замедляем создание
         * потоков. Это нужно, чтобы избежать ситуации, когда все 10 потоков-читателей используют
         * ресурс одновременно, не освобождая его для писателя.
         */
        usleep(reader_count * 200000);
    }

    // Ждем, когда потоки закончат свою работу
    for (int i = 0; i < 10; i++) {
        pthread_join(readers[i], nullptr);
    }
    for (int i = 0; i < 10; i++) {
        pthread_join(writers[i], nullptr);
    }

    // Уничтожаем семафоры
    sem_destroy(&reader_count_sem);
    sem_destroy(&resource_sem);

    return 0;
}