# Запуск

## Клонирование репозитория

```
git clone https://github.com/halingeorge/hash-table.git
git submodule init
git submodule update
```

## Запуск тестов и бенчмарков
```
gkhalin@gkhalin:~/dynamic-hash-table$ ./run_tests.py --help
usage: run_tests.py [-h] [--asan] [--tsan] [--benchmark] [--only-benchmark]

optional arguments:
  -h, --help        show this help message and exit
  --asan, -a        run tests under asan
  --tsan, -t        run tests under tsan
  --benchmark, -b   run benchmark after unittests
  --only-benchmark  run benchmark without unittests
```


