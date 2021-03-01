/*
   Test timing for big switch statements

   g++ -Wall -O3 /home/will/yoshimi-code/Util/switch_time.cpp -o /home/will/yoshimi-code/Util/switch_time

   try with and without O3 optimisation!
*/

#include <stdio.h>
#include <sys/time.h>
# include <iostream>

using namespace std;

int packed(int test)
{
    struct timeval tv1, tv2;
    int result = 90;
    gettimeofday(&tv1, NULL);
    for (int i = 0; i < 2000; ++i) // takes about 4 seconds
    {
        for (int j = 0; j < 1000000; ++j)
        {
           switch (test)
           {
                case 0:
                   result += 5;
                   break;
                case 1:
                   result += 2;
                   break;
                case 2:
                   result += 7;
                   break;
                case 3:
                   result -= 5;
                   break;
                case 4:
                   result += 20;
                   break;
                case 5:
                   result -= 35;
                   break;
                case 6:
                   result += 1;
                   break;
                case 7:
                   result = 555;
                   break;
                case 8:
                   result -= 7;
                   break;
                case 9:
                   result -= 33;
                   break;
                case 10:
                   result = 28;
                   break;
                case 11:
                   result = 59;
                   break;
                case 12:
                   result += 16;
                   break;
                case 13:
                   result += 4;
                   break;
                case 14:
                   result -= 33;
                   break;
                case 15:
                   result = 19;
                   break;
                case 16:
                   result += 16;
                   break;
                case 17:
                   result += 4;
                   break;
                case 18:
                   result -= 33;
                   break;
                case 19:
                   result = 19;
                   break;
                case 20:
                   result += 5;
                   break;
                case 21:
                   result += 2;
                   break;
                case 22:
                   result += 7;
                   break;
                case 23:
                   result -= 5;
                   break;
                case 24:
                   result += 20;
                   break;
                case 25:
                   result -= 35;
                   break;
                case 26:
                   result += 1;
                   break;
                case 27:
                   result = 555;
                   break;
                case 28:
                   result -= 7;
                   break;
                case 29:
                   result -= 33;
                   break;
                case 30:
                   result = 28;
                   break;
                case 31:
                   result = 59;
                   break;
                case 32:
                   result += 16;
                   break;
                case 33:
                   result += 4;
                   break;
                case 34:
                   result -= 33;
                   break;
                case 35:
                   result = 19;
                   break;
                case 36:
                   result += 16;
                   break;
                case 37:
                   result += 4;
                   break;
                case 38:
                   result -= 33;
                   break;
                case 39:
                   result = 19;
                   break;
                case 40:
                   result += 5;
                   break;
                case 41:
                   result += 2;
                   break;
                case 42:
                   result += 7;
                   break;
                case 43:
                   result -= 5;
                   break;
                case 44:
                   result += 20;
                   break;
                case 45:
                   result -= 35;
                   break;
                case 46:
                   result += 1;
                   break;
                case 47:
                   result = 555;
                   break;
                case 48:
                   result -= 7;
                   break;
                case 49:
                   result -= 33;
                   break;
                case 50:
                   result = 28;
                   break;
                case 51:
                   result = 59;
                   break;
                case 52:
                   result += 16;
                   break;
                case 53:
                   result += 4;
                   break;
                case 54:
                   result -= 33;
                   break;
                case 55:
                   result = 19;
                   break;
                case 56:
                   result += 16;
                   break;
                case 57:
                   result += 4;
                   break;
                case 58:
                   result -= 33;
                   break;
                case 59:
                   result = 19;
                   break;
                case 60:
                   result += 5;
                   break;
                case 61:
                   result += 2;
                   break;
                case 62:
                   result += 7;
                   break;
                case 63:
                   result -= 5;
                   break;
                case 64:
                   result += 20;
                   break;
                case 65:
                   result -= 35;
                   break;
                case 66:
                   result += 1;
                   break;
                case 67:
                   result = 555;
                   break;
                case 68:
                   result -= 7;
                   break;
                case 69:
                   result -= 33;
                   break;
                case 70:
                   result = 28;
                   break;
                case 71:
                   result = 59;
                   break;
                case 72:
                   result += 16;
                   break;
                case 73:
                   result += 4;
                   break;
                case 74:
                   result -= 33;
                   break;
                case 75:
                   result = 19;
                   break;
                case 76:
                   result += 16;
                   break;
                case 77:
                   result += 4;
                   break;
                case 78:
                   result -= 33;
                   break;
                case 79:
                   result = 19;
                   break;
           }
        }
    }
    gettimeofday(&tv2, NULL);
    if (tv1.tv_usec > tv2.tv_usec)
    {
        tv2.tv_sec--;
        tv2.tv_usec += 1000000;
    }
    cout << "Time packed " << tv2.tv_sec - tv1.tv_sec << "." << tv2.tv_usec - tv1.tv_usec << endl;
    return result;
}

int sparse(int test)
{
    struct timeval tv1, tv2;
    int result = 90;
    gettimeofday(&tv1, NULL);
    for (int i = 0; i < 2000; ++i) // takes about 4 seconds
    {
        for (int j = 0; j < 1000000; ++j)
        {
           switch (test)
           {
                case 0:
                   result += 5;
                   break;
                case 10:
                   result += 2;
                   break;
                case 11:
                   result += 7;
                   break;
                case 12:
                   result -= 5;
                   break;
                case 40:
                   result += 20;
                   break;
                case 47:
                   result -= 35;
                   break;
                case 60:
                   result += 1;
                   break;
                case 61:
                   result = 555;
                   break;
                case 93:
                   result -= 7;
                   break;
                case 100:
                   result -= 33;
                   break;
                case 132:
                   result = 28;
                   break;
                case 137:
                   result = 59;
                   break;
                case 143:
                   result += 16;
                   break;
                case 147:
                   result += 4;
                   break;
                case 162:
                   result -= 33;
                   break;
                case 181:
                   result = 19;
                   break;
                case 216:
                   result += 16;
                   break;
                case 417:
                   result += 4;
                   break;
                case 458:
                   result -= 33;
                   break;
                case 509:
                   result = 19;
                   break;
                case 600:
                   result += 5;
                   break;
                case 610:
                   result += 2;
                   break;
                case 711:
                   result += 7;
                   break;
                case 712:
                   result -= 5;
                   break;
                case 740:
                   result += 20;
                   break;
                case 847:
                   result -= 35;
                   break;
                case 860:
                   result += 1;
                   break;
                case 961:
                   result = 555;
                   break;
                case 993:
                   result -= 7;
                   break;
                case 1010:
                   result -= 33;
                   break;
                case 1132:
                   result = 28;
                   break;
                case 1237:
                   result = 59;
                   break;
                case 1243:
                   result += 16;
                   break;
                case 1647:
                   result += 4;
                   break;
                case 1962:
                   result -= 33;
                   break;
                case 2101:
                   result = 19;
                   break;
                case 2116:
                   result += 16;
                   break;
                case 2417:
                   result += 4;
                   break;
                case 2458:
                   result -= 33;
                   break;
                case 2509:
                   result = 19;
                   break;
                case 5000:
                   result += 5;
                   break;
                case 5010:
                   result += 2;
                   break;
                case 5011:
                   result += 7;
                   break;
                case 5012:
                   result -= 5;
                   break;
                case 5040:
                   result += 20;
                   break;
                case 5047:
                   result -= 35;
                   break;
                case 5060:
                   result += 1;
                   break;
                case 5061:
                   result = 555;
                   break;
                case 5093:
                   result -= 7;
                   break;
                case 5100:
                   result -= 33;
                   break;
                case 5132:
                   result = 28;
                   break;
                case 5137:
                   result = 59;
                   break;
                case 5143:
                   result += 16;
                   break;
                case 5147:
                   result += 4;
                   break;
                case 5162:
                   result -= 33;
                   break;
                case 5181:
                   result = 19;
                   break;
                case 5216:
                   result += 16;
                   break;
                case 5417:
                   result += 4;
                   break;
                case 5458:
                   result -= 33;
                   break;
                case 5509:
                   result = 19;
                   break;
                case 5600:
                   result += 5;
                   break;
                case 5610:
                   result += 2;
                   break;
                case 5711:
                   result += 7;
                   break;
                case 6712:
                   result -= 5;
                   break;
                case 6740:
                   result += 20;
                   break;
                case 6847:
                   result -= 35;
                   break;
                case 6860:
                   result += 1;
                   break;
                case 6961:
                   result = 555;
                   break;
                case 6993:
                   result -= 7;
                   break;
                case 7010:
                   result -= 33;
                   break;
                case 7132:
                   result = 28;
                   break;
                case 7237:
                   result = 59;
                   break;
                case 7243:
                   result += 16;
                   break;
                case 7647:
                   result += 4;
                   break;
                case 7962:
                   result -= 33;
                   break;
                case 8101:
                   result = 19;
                   break;
                case 8116:
                   result += 16;
                   break;
                case 8417:
                   result += 4;
                   break;
                case 8458:
                   result -= 33;
                   break;
                case 8509:
                   result = 19;
                   break;
           }
        }
    }
    gettimeofday(&tv2, NULL);
    if (tv1.tv_usec > tv2.tv_usec)
    {
        tv2.tv_sec--;
        tv2.tv_usec += 1000000;
    }
    cout << "Time sparse " << tv2.tv_sec - tv1.tv_sec << "." << tv2.tv_usec - tv1.tv_usec << endl;
    return result;
}

int main()
{
    int a, b, c, d, e, f;
    cout << "\n first" << endl;
    a = packed(0);
    b = sparse(0);
    cout << "\n middle" << endl;
    c = packed(40);
    d = sparse(5000);
    cout << "\n last" << endl;
    e = packed(78);
    f = sparse(8509);
    cout << "\n\n a-f" << a << " "  << b << " "  << c << " "  << d << " "  << e << " "  << f << endl;
    return 0;
    // results are not used
    // just there to fool compiler
}
