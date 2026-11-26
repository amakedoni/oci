# Лабораторная работа № 2

Из папки `src` запустите Ubuntu.

```sh
docker run --rm -it -v "$PWD:/src" -w /tmp ubuntu:24.04 bash
```

В контейнере установите пакеты, соберите и запустите программу.

```sh
apt-get update && apt-get install -y gcc libc6-dev make strace
cp /src/Makefile /src/*.c .
make && strace -f -o /src/strace.log ./parent
```

Введите `result.txt`, затем строки `100 2 5` и `10 0`. Трассировка сохранится в `src/strace.log`.
