#!/usr/bin/python3
# reorder SQLite `file.sql` to `stable-file.sql`
# usage `./sql-stab.py file.sql`
import operator
import sys
import os
import re


def order_sql(filename):
    with open(filename, 'r') as file:
        folder, filename = os.path.split(filename)
        filename_out = os.path.join(folder, "stable-" + filename)
        with open(filename_out, 'w') as f:
            lines = file.readlines()

            sql = ''
            tables = []

            for line in lines:
                sql += line
                if line.strip().endswith(';'):
                    if sql.startswith('CREATE TABLE'):
                        # sql = sql.replace("CREATE TABLE IF NOT EXISTS", "CREATE TABLE")
                        tables.append(Table(sql))
                    elif sql.startswith('INSERT INTO'):
                        if len(tables) > 0 and tables[-1]:
                            tables[-1].inserts.append(sql)
                        else:
                            # 写入当前语句
                            f.write(sql)
                    else:
                        # 结束了大段的table/insert交替语句
                        if tables:
                            cmp_table = operator.attrgetter('name')
                            tables.sort(key=cmp_table)  # 对table排序
                            for table in tables:
                                # print(table.name)
                                f.write(table.create)
                                table.inserts.sort()  # 对insert进行排序
                                for insert in table.inserts:
                                    f.write(insert)
                            tables = []
                        # 写入当前语句
                        f.write(sql)

                    sql = ''


class Table:
    def __init__(self, c):
        self.create = c
        self.inserts = []

        match = re.match(r'CREATE\s+TABLE\s+IF\s+NOT\s+EXISTS\s+([^\s\(]+)\s*', c, re.IGNORECASE)
        if match:
            self.name = match.group(1).replace('"', '')
        else:
            match = re.match(r'CREATE\s+TABLE\s+([^\s\(]+)\s*', c, re.IGNORECASE)
            if match:
                self.name = match.group(1).replace('"', '')
            else:
                self.name = c


if __name__ == '__main__':
    if len(sys.argv) == 2:
        order_sql((sys.argv[1]))
    else:
        print('请输入sql文件名，作为第一个参数')

