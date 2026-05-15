// Пример 1: Обработка всех исключений
try
    x = 10 / 0
failure
    os.output("Ошибка: {error}")

// Пример 2: Обработка конкретного типа
try
    arr = (1, 2, 3)
    value = arr.5  // IndexError
failure zerodivision
    os.output("Деление на ноль")
failure index
    os.output("Выход за границы массива: {error}")
failure
    os.output("Другая ошибка: {error_type} - {error}")

// Пример 3: С вложенными обработчиками
try
    try
        file = open("nonexistent.txt")
        data = file.read()
    failure filenotfound
        os.output("Файл не найден!")
    failure permission
        os.output("Нет прав доступа!")
failure io
    os.output("Ошибка ввода-вывода")

// Пример 4: С always блоком
try
    counter = 0
    while counter < 10
        counter = counter + 1
        if counter == 5
            break
    os.output("Цикл завершен")
failure keyboardinterrupt
    os.output("Программа прервана пользователем")
always
    os.output("Это выполнится всегда")