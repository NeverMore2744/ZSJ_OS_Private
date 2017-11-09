# coding: utf-8
import os
import sys
import time


def get_line_count(file_path):
    """获取单个文件的行数"""
    with open(file_path) as f:
        return len(f.readlines())


def get_children_paths(path):
    """获取给定路径下所有子文件路径"""
    res = []
    for r_path in os.walk(path):
        for f_name in r_path[2]:
            res.append(os.path.join(r_path[0], f_name))
    return res


def main(*args):
    """
    获取指定目录下的指定格式的文件数量及行数
    @command: python tj.py /home/test/ py txt
    @param: args 直接执行文件时的 sys.argv
    """
    assert len(sys.argv) >= 3, u'''Syntax Error, expect at least 2 args, %d given \n
     Command like this: python tj.py /home/test/ py txt''' % (len(sys.argv) - 1)

    start_time = time.time()
    root_path = args[1]
    suffix = args[2:]
    line_count = 0
    file_count = 0

    if os.path.isdir(root_path):
        paths = get_children_paths(root_path)
    else:
        paths = [root_path]
    for file_path in paths:
        if os.path.splitext(file_path)[1].lstrip('.') in suffix:
            file_count += 1
            line_count += get_line_count(file_path)
    print u'file count is %d' % file_count
    print u'line count is %d' % line_count
    print u'complete with %.4f seconds' % (time.time() - start_time)


if __name__ == '__main__':
    main(*sys.argv)
