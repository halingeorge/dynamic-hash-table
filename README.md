# Запуск

## Клонирование репозитория

```
git clone https://github.com/halingeorge/dynamic-hash-table.git
git submodule init
git submodule update
```

## Запуск тестов и бенчмарков
```
gkhalin@gkhalin:~/dynamic-hash-table$ ./run_tests.py --help
usage: run_tests.py [-h] [--asan] [--tsan] [--benchmark] [--benchmark-debug]

optional arguments:
  -h, --help         show this help message and exit
  --asan, -a         run tests under asan
  --tsan, -t         run tests under tsan
  --benchmark, -b    run benchmark
  --benchmark-debug  run benchmark in debug mode
```


