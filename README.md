# Домашняя работа #1, удаленный калькулятор

## Автор
Кравченко Дмитрий, 3й курс

## Формат ввода выражений
Я использую префиксную запись, просто чтобы было легче парсить. Пара примеров:

```
> + 1 2
> 3

> + 1 2 3 4 5
> 15

> mult 1 2 3
> 6

> fib 7
> 13
```

Доступные операции -- `+ (plus)`, `- (minus)`, `* (mult)`, `/ (div)`, `fib`, `fact`

Я оперирую везде 32-х битными целыми числами (`int32_t`). Т.е. результат деления 1 на 2 будет 0.

## Асинхронные операции
`fact` и `fib`, как и требуется в задании, асинхронные. Но для того, чтобы сделать их асинхронными,
я создаю новое подключение, в рамках которого и выполняю требуемую операцию. Я не придумал, как сохраняя
интрфейс наших сокетов легко делать это все в рамках одного подключения. Единственный способ, который я придумал,
это делать `poll` сокета и `stdin`'a. 

Если же мы ждем ввода юзера с помощью просто `getline`, то в то время, когда юзер только вводит свой запрос, нам
не получить ответа на асинхронную операцию. Конечно, мы бы могли создать изначально поток в background'e, который бы
делал `recv` на сокете и выводил то, что получает, а наш `main thread` делал бы `send`, но тогда не очень понятно, как
нам делать остальные операции блокирующими. Надо как-то было бы для каждого сообщения вводить ID-шник и блокировать `main thread`,
пока background'ный не получит `recv` с нужным ID, но как-то это все сложно.

**TL;DR**

Я выбрал самый топорный способ сделать блокирующие и неблокирующие запросы -- `+`, `-`, `*`, `/` работают с "основным" поключением
и делают там `send/recv`, для `fib` и `fact` я тупо создаю `pthread`, в котором тупо создаю новое подключение, где и делаю `send/recv`.

## Сборка
`./build.sh` все соберет. Можно просто `make`-ом, но тогда лучше сделать `make debug`, т.к. иначе не будет логирования.
