# Лабораторная работа № 3

Из папки `src` запустите Ubuntu.

```sh
docker run --rm -it -v "$PWD:/src" -w /tmp ubuntu:24.04 bash
```

В контейнере установите пакеты, соберите и запустите программу.

```sh
apt-get update && apt-get install -y gcc libc6-dev make strace
cp /src/Makefile /src/*.c .
sed -i 's/pthread_threadid_np(NULL, &tid);/tid = gettid();/' child.c
make CC='gcc -D_GNU_SOURCE' && strace -f -o /src/strace.txt ./lab3 -t 4
```

`sed` меняет только копию `child.c` внутри контейнера. Для примера введите по строкам `4`, `1 1`, `10 10`, `2 2`, `12 12`, `2`. Трассировка сохранится в `src/strace.txt`.
