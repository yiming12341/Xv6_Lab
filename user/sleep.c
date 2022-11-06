#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/*
��������
argc : main��������������������Ϊvoid��ʱ��argc=1��Ĭ�ϲ���Ϊ��ִ���ļ���
argv : ָ�����飬�ֱ�ָ��������ַ����׵�ַ������argv[0]ָ��Ĭ�ϲ���
�˷�����Ҫ�����������������
Ĭ������£�argc ֵΪ 1����ʾ argv[0] ��ǰ���̿�ִ���ļ����ļ�����
����Ĳ����� arg[1] ��ʼ����˳�������ַ������У�argc ��������1����
*/
int main(int argc,char const* argv[])
{
    
    int n;
    if (argc != 2)
    {
        fprintf(2, "usage:Parameter is incorrect\n");
         exit(1);
    }
    else
    {
        n = atoi(argv[1]);
        sleep(n);
        exit(0);
    }

}