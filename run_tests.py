#!/usr/bin/python3
import argparse
import os
import subprocess
import tempfile


def get_build_dir_name(cxx_flag):
    if not cxx_flag:
        return 'build'
    elif cxx_flag == '-fsanitize=address':
        return 'asan_build'
    return 'tsan_build'


def run_and_exit_if_fail(command, exit_if_error=True):
    print(command)

    with tempfile.TemporaryDirectory() as temp_dir:
        output_file = f'{temp_dir}/output'

        try:
            subprocess.run(command + f' 2>&1 | tee -a {output_file};', shell=True, check=True)
        except:
            if exit_if_error:
                raise
            return

        with open(output_file, 'r') as file:
            output = file.read()
            if exit_if_error and 'error:' in output.lower():
                exit(1)
    print('Done')


def run_benchmark(build_type='release', cxx_flag=''):
    run_and_exit_if_fail(f'mkdir {build_type}_benchmark_build', exit_if_error=False)
    cur_dir = os.getcwd()
    os.chdir(f'{build_type}_benchmark_build')
    run_and_exit_if_fail(f'cmake -DCMAKE_BUILD_TYPE={build_type} -DCMAKE_CXX_FLAGS="{cxx_flag}" ..')
    run_and_exit_if_fail('make -j4')
    run_and_exit_if_fail('benchmark_tests/hash_table_benchmark')
    os.chdir(cur_dir)


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument('--asan', '-a', action='store_true', help='run tests under asan')
    parser.add_argument('--tsan', '-t', action='store_true', help='run tests under tsan')
    parser.add_argument('--benchmark', '-b', action='store_true', help='run benchmark')
    parser.add_argument('--benchmark-debug', action='store_true', help='run benchmark in debug mode')

    args = parser.parse_args()

    cxx_flags = []
    if args.asan:
        cxx_flags.append('-fsanitize=address')
    if args.tsan:
        cxx_flags.append('-fsanitize=thread')
    if args.benchmark:
        run_benchmark('release', '')
        return
    if args.benchmark_debug:
        run_benchmark('debug', '-fsanitize=address')
        return

    if not cxx_flags:
        cxx_flags = ['']

    for cxx_flag in cxx_flags:
        build_dir_name = get_build_dir_name(cxx_flag)
        run_and_exit_if_fail(f'mkdir {build_dir_name}', exit_if_error=False)
        cur_dir = os.getcwd()
        os.chdir(build_dir_name)
        run_and_exit_if_fail(f'cmake -DCMAKE_BUILD_TYPE=debug -DCMAKE_CXX_FLAGS=\"{cxx_flag}\" ..')
        run_and_exit_if_fail('make -j4')
        run_and_exit_if_fail('unit_tests/hash_table_test')
        os.chdir(cur_dir)


if __name__ == '__main__':
    main()
