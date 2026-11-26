# Лабораторная работа № 4

В терминале macOS откройте `lab04/src` и запустите Ubuntu.

```sh
ls Makefile && docker run --rm -it -v "$PWD:/src" -w /tmp ubuntu:24.04 bash
```

В контейнере установите пакеты, соберите и запустите программу.

```sh
apt-get update && apt-get install -y gcc libc6-dev make strace
cp /src/Makefile /src/*.c .
make && strace -f -o /src/strace.txt ./main
```

Введите `result.txt`, затем строки `20 2 5`, `60 3 2` и `12 0`. Трассировка сохранится в `src/strace.txt`.
