/* tc.c file */
extern int g;
int h;
main()
{
	int a, b, c, *bp;
	g = 100;
	bp = getbp();
	a = 1; b = 2; h = 200;
	c = mysum(a, b);
	printf("a=%d b=%d c=%d\n", a, b, c);
}