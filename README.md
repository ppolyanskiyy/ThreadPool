
# Пул Потоків у С++
Це бібліотека пулу потоків у С++, яка дозволяє легко управляти багатопоточними завданнями. 
Пул потоків дозволяє ефективно використовувати доступні потоки для обробки різних завдань паралельно, замість створення нового потоку для кожного завдання. 
Це забезпечує більшу продуктивність та оптимальне використання ресурсів.

## Властивості
- Динамічне управління потоками: бібліотека автоматично керує створенням та знищенням потоків залежно від навантаження.
- Керування чергами завдань: пул потоків управляє чергою завдань, що чекають на виконання, та призначає їх доступним потокам.
- Використання різниз алгоритмів планування: для пулу можна встановити декілька популярних алгоритмів планування (FCFS, SJF, Пріоритетне).
- Підтримка параметрів потоку: можливість встановлювати кількість потоків у пулі, мінімиальну та максимальну кількість потоків, час очікування та інші параметри.
- Зручний API: бібліотека надає простий та інтуїтивно зрозумілий інтерфейс для створення та запуску завдань у пулі потоків.
- Розширюваність: можна легко розширити базовий функціонал.

## Залежності
- CMake 3.2 і вищий.
- (опційно) gTest. Бібліотека повинна знаходитись в ThirdParty папці.

## Встановлення
1. Склонуйте репозиторій до вашого локального середовища: ```git clone https://github.com/ppolyanskiyy/ThreadPool.git```
2. Збудуйте бібліотеку із використанням CMake: ```python build.py```
3. Підключіть "ThreadPool.h" до вашого проєкту: ```#include "inc/ThreadPool/ThreadPool.h"```
4. Підключіть бажане завдання для пулу "ThreadPoolTask.h" до вашого проєкту: ```#include "inc/ThreadPoolTask/ThreadPoolTask.h"```
5. (опційно) Підключіть допоміжний клас "ThreadPoolOptionsBuilder.h "для створення параметрів пулу: ```#include "inc/ThreadPool/ThreadPoolOptionsBuilder.h ```

## Використання
```
#include <ThreadPool.h>
#include <ThreadPoolTask.h>
#include <ThreadPoolOptionsBuilder.h>

int sum(int a, int b)
{
	return a + b;
}

int main()
{
	ThreadPoolOptions options = ThreadPoolOptionsBuilder(3)
		.setMaxNumberOfWorkers(5)
		.setMinNumberOfWorkers(1)
		.setPostponeExecution(false)
		.setSchedulerType(ThreadPoolOptions::SchedulerType::FCFS)
		.setWaitAllTasksExecutionFinished(true)
		.build();

	ThreadPool threadPool{ options };
	std::shared_ptr<ThreadPoolTask> task{ std::make_shared<ThreadPoolTask>() };
	auto futureResult = task->submitOne(sum, 1, 3);

	threadPool.addTask(task);
	threadPool.waitAllTasksExecutionFinished(-1);

	int result = futureResult.get();

	std::cout << result;
}

```

## Внесок та вдосконалення
Якщо у вас є пропозиції щодо вдосконалення цієї бібліотеки або ви знайшли помилку, будь ласка, створіть pull-запит або відкрийте питання у розділі Issues. Ваш внесок є вельми важливим!

