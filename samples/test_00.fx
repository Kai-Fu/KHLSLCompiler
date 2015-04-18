// This is comments..
/* comments   */
float TestFunc_0(float l, float r) 
{
	return pow(l, r);
}
/*


*/

/* another type of comments
  1. bla bla
  2. bla 
  
 */
 
extern MyExtType;
extern MyExtType1;

float ExternFunc(MyExtType arg)
{
	MyExtType1 tmp = arg;
	MyExtType1 tmp1[3];
	tmp1[1] = arg;
	bool tmpB = true;
	bool tmpA = !tmpB;
	return 123;
}

struct MyTestStruct
{
	float3 a;
	float2 b;
	int c;
	int4 d;
	bool2 e;
	float8 f;
};

float TestFunc_1(float l) 
{
	return sqrt(l);
}

#define MY_VALUE 123.45
void run_test()
{

	float res0 = TestFunc_0(2, MY_VALUE);
	float res1 = TestFunc_1(2);
	CompareTwoInt(res0*1000, res1*1000);
}

/* comment at the end of file 
 */