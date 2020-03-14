# Запуск

## Клонирование репозитория

```
git clone https://github.com/halingeorge/hash-table.git
git submodule init
git submodule update
```

## Сборка

```
mkdir build
cd build
cmake ..
make
```

## Сборка с TSAN (проверено на clang 9)
```
mkdir build; cd build; cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread"; make -j4
```

## Запуск бенчмарка

```
./unit_tests/hash_table_test
```


