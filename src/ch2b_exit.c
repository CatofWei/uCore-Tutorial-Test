#include <stdio.h>
#include <unistd.h>
const int MAGIC = 1234;
/// 正确输出： 不输出 FAIL，以 1234 退出
int main()
{
	exit(MAGIC);
	int result = puts("exiting!!\n");
	return 0;
}