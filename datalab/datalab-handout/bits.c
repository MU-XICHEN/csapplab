/*
 * CS:APP Data Lab
 *
 * <Please put your name and userid here>
 *
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting if the shift amount
     is less than 0 or greater than 31.


EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implement floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants. You can use any arithmetic,
logical, or comparison operations on int or unsigned data.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operations (integer, logical,
     or comparison) that you are allowed to use for your implementation
     of the function.  The max operator count is checked by dlc.
     Note that assignment ('=') is not counted; you may use as many of
     these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */

#endif
// 1
/*
 * bitXor - x^y using only ~ and &
 *   Example: bitXor(4, 5) = 1
 *   Legal ops: ~ &
 *   Max ops: 14
 *   Rating: 1
 */
int bitXor(int x, int y)
{
  // 得到都是1的部分(x & y)，和都是0的部分(~x & ~y)，然后取剩余部分就好
  return (~(x & y) & ~(~x & ~y));
}
/*
 * tmin - return minimum two's complement integer
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmin(void)
{
  return 1 << 31;
}
/*
 * isTmax - returns 1 if x is the maximum, two's complement number,
 *     and 0 otherwise
 *   Legal ops: ! ~ & ^ | +
 *   Max ops: 10
 *   Rating: 1
 */
int isTmax(int x)
{
  // 如果最大或-1，有性质，x ^ (x + 1) == 0xFFFFFFFF
  return !(~((x) ^ (x + 1))) & !!(~x);
}
/*
 * allOddBits - return 1 if all odd-numbered bits in word set to 1
 *   where bits are numbered from 0 (least significant) to 31 (most significant)
 *   Examples allOddBits(0xFFFFFFFD) = 0, allOddBits(0xAAAAAAAA) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 2
 */
int allOddBits(int x)
{
  // 0xAAAAAAAA 掩码可以通过 | 构造出来
  int mask;
  mask = 0xAA | (0xAA << 8);
  mask = mask | (mask << 16);
  return !((mask & x) ^ mask);
}
/*
 * negate - return -x
 *   Example: negate(1) = -1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int negate(int x)
{
  return ~x + 1;
}
// 3
/*
 * isAsciiDigit - return 1 if 0x30 <= x <= 0x39 (ASCII codes for characters '0' to '9')
 *   Example: isAsciiDigit(0x35) = 1.
 *            isAsciiDigit(0x3a) = 0.
 *            isAsciiDigit(0x05) = 0.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 3
 */
int isAsciiDigit(int x)
{
  // AsciiDigit位模式有两种：（1）0.....0110xxx; (2) 0....011100x
  return !((x >> 3) ^ 0x6) | !((x >> 1) ^ 0x1c);
}
/*
 * conditional - same as x ? y : z
 *   Example: conditional(2,4,5) = 4
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
int conditional(int x, int y, int z)
{
  // 当 !!x 为1，时，其逆元对应-1（0xffffffff），这个时候与其&，得到原值
  // 此时，!x 为0，逆元依旧是 0，& 结果则是0
  return ((~(!!x) + 1) & y) | ((~(!x) + 1) & z);
}
/*
 * isLessOrEqual - if x <= y  then return 1, else return 0
 *   Example: isLessOrEqual(4,5) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isLessOrEqual(int x, int y)
{
  // 结合 conditional
  // 通过 (x >> 31) ^ (y >> 31) 的结果进行符号判断
  // 不同符时，只有 x 为负数时满足 x <= y
  // 同符时，根据相减的结果的正负判断
  // x 和 y 相等时另算
  int equal = !(x ^ y);
  int a = (x >> 31) ^ (y >> 31); // a 为 0 时，不同符；a 为 0xffffffff 时，不同符
  int lhs = a & !!(x >> 31); // 目的：a 为 11..11 的时候（不同符），且 x 为负数时，得到 1
  int rhs = ((~(!a) + 1) & !!((x + (~y + 1)) >> 31)); // 目的：a 为 00..00（同符），根据相减结果判断大小
  int res = equal | lhs | rhs;
  return res;
}
// 4
/*
 * logicalNeg - implement the ! operator, using all of
 *              the legal operators except !
 *   Examples: logicalNeg(3) = 0, logicalNeg(0) = 1
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 4
 */
int logicalNeg(int x)
{
  // 除了 0，任意成对的 x 和 -x，其中一方都有一个最高位的 1, 所以只要满足所有的转化为 0xffffffff 即可
  // 0xffffffff + 1溢出变成0
  return ((x | (~x + 1)) >> 31) + 1;
}
/* howManyBits - return the minimum number of bits required to represent x in
 *             two's complement
 *  Examples: howManyBits(12) = 5
 *            howManyBits(298) = 10
 *            howManyBits(-5) = 4
 *            howManyBits(0)  = 1
 *            howManyBits(-1) = 1
 *            howManyBits(0x80000000) = 32
 *  Legal ops: ! ~ & ^ | + << >>
 *  Max ops: 90
 *  Rating: 4
 */
int howManyBits(int x)
{

  /**
   * 思路二分法
   * 右移 16 个，如果为0，则表示位数小于等于16；不为0，则表示需要的位数比 16 位要大
   * 依次二分往下
   * 注意，负数从 11...11起步，表示 -1，等价于 一个 bit 1 即可表示-1
   * 正数从 00...01 起步，表示 1，等价于 2个 bit 01 即可表示 1
   * 0 占了正数最开始的位模式，所以负数的所需bit，等价于其逆元的前一位
   * 所需对应bit如下：
   *   -4 -3 -2 -1 0 1 2 3 4
   *    3  3  2  1 1 2 3 3 4
   */

  int sign = x >> 31;
  int b16, b8, b4, b2, b1, b0, sum;

  x = ((~(!sign) + 1) & x) | ((~(!!sign) + 1) & ~x); // 正数本身，负数构建逆元前一位

  b16 = !!(x >> 16) << 4;
  x >>= b16;

  b8 = !!(x >> 8) << 3;
  x >>= b8;

  b4 = !!(x >> 4) << 2;
  x >>= b4;

  b2 = !!(x >> 2) << 1;
  x >>= b2;

  b1 = !!(x >> 1);
  x >>= b1;

  b0 = x;

  sum = (b16 + b8 + b4 + b2 + b1 + b0 + 1); // 1 表示任意一个正数的符号所需额外占1

  return sum;
}
// float
/*
 * floatScale2 - Return bit-level equivalent of expression 2*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned floatScale2(unsigned uf)
{
  /**
   * 分情况：
   * 概念：
   *      (1) 规格化数，阶数部分不为 0 或 255，
   *      (2) 非规格化数，阶数部分为 0
   * 1️⃣ 规格化数，阶数 + 1来实现
   * 2️⃣ 非规格化数，整体 << 1
   */
  unsigned res = 0;
  unsigned signFrag = 0x80000000 & uf, eFrag = 0x7f800000, fFrag = 0x7fffff;
  unsigned dataFrag = eFrag | fFrag;

  if (!((eFrag & uf) ^ eFrag))
  {
    // NaN or 无穷数
    return uf;
  }

  if (!(uf & eFrag))
  {
    // 非规格化数，通过 << 1 来实现
    return signFrag | ((uf << 1) & dataFrag);
  }
  else
  {
    // 规格化数，通过 eFragment + 1来实现
    res = (uf >> 23) + 1;
    if ((res & 0xff) >= 0xff)
    {
      // 注意，这里可能导致 eFrag 变成 无穷大
      return signFrag | eFrag;
    }
    else
    {
      return signFrag | ((res << 23) & eFrag) | (uf & fFrag);
    }
  }
}
/*
 * floatFloat2Int - Return bit-level equivalent of expression (int) f
 *   for floating point argument f.
 *   Argument is passed as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point value.
 *   Anything out of range (including NaN and infinity) should return
 *   0x80000000u.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
int floatFloat2Int(unsigned uf)
{
  /**
   * 也是分情况，规格化数和非规格化数
   * 1️⃣ 非规格化数，数值部分永远小于1，毕竟 M的部分是0.f，E的部分始终是1 - bias，二者相乘（M * 2的E次幂）< 1
   * 2️⃣ 规格化数，根据指数部分、尾数部分转化成二进制后，因为‼️向0舍入，所以多余的小数部分统一抛弃
   * - 例如，十进制中 1.35e1 实际小数部分是 0.5；float 转 int 同理，尾数部分在 ✖️ 2的E方 后，抛弃多余的小数部分
   * - 记住，规格化数有个隐含的1，转化为最终int的时候，这个1也要记得同步移动，参考规格化数部分
   */
  unsigned res = 0;
  unsigned bias = 0x7f;
  unsigned signFrag = 0x80000000 & uf, eFrag = 0x7f800000, fFrag = 0x7fffff;
  unsigned minusBias = 0, eCount = 0;
  if (!((eFrag & uf) ^ eFrag))
  {
    // NaN or 无穷数
    return 0x80000000u;
  }

  if (!(uf & eFrag))
  {
    // 非规格化数，直接向 0 舍入
    return 0;
  }
  else
  {
    /**
     * 规格化数
     */
    eCount = (eFrag & uf) >> 23;
    minusBias = eCount - bias;
    if (eCount >= bias)
    {
      if (minusBias > 30)
      {
        // 超出范围
        return 0x80000000u;
      }
      else if (minusBias <= 30 && minusBias > 23)
      {
        // 最终 int 位模式会生成为 01xx..xx，其中 xx 部分就是由小数部分左移得到
        res = (1 << (minusBias)) + ((uf & fFrag) << ((minusBias) - 23));
      }
      else
      {
        // E 不超过23时，尾数本身尾数就足够了，而且还需要向右移，从而得到对应的值
        res = (1 << (minusBias)) + ((uf & fFrag) >> (23 - (minusBias)));
      }
    }
    else
    {
      // E 为负数时
      res = 0; // 此时规格化，隐含的 1，且 E 为负数，1.f 的E次方一定不大于1
    }

    // 浮点数的正负数只有符号不同，转换为int时，要转化为补码形式
    return (!signFrag) ? res : (~res + 1);
  }
}

/*
 * floatPower2 - Return bit-level equivalent of the expression 2.0^x
 *   (2.0 raised to the power x) for any 32-bit integer x.
 *
 *   The unsigned value that is returned should have the identical bit
 *   representation as the single-precision floating-point number 2.0^x.
 *   If the result is too small to be represented as a denorm, return
 *   0. If too large, return +INF.
 *
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. Also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned floatPower2(int x)
{
  unsigned bias = 0x7f;
  if (x >= -126 && x <= 127) {
    return (x + bias) << 23;
  } else if(x < -126) {
    return 0;
  } else {
    return 0x7f800000;
  }
}
