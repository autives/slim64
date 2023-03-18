#define MAX_INT_LENGTH 11
#define MAX_FLOAT_LENGTH 12
#define FLOAT_PRECISION 6

int _fltused;

static const double _pow_of_10[] = {1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f, 100000.0f, 1000000.0f};
static const double _round_threashold[] = {0, 0.5f, 0.45f, 0.445f, 0.4445f, 0.44445f};

static inline int IsNan(double num) {
    int value = *(int *)(&num);
    int significand = (value) & 0b00000000011111111111111111111111;
    unsigned char exponenent = ((value) & ~(0b00000000011111111111111111111111)) >> 23;

    return exponenent == 0xff && significand != 0; 
}

static inline int IsInf(double num){
    int value = *(int *)(&num);
    int significand = (value) & 0b00000000011111111111111111111111;
    unsigned char exponenent = ((value) & ~(0b00000000011111111111111111111111)) >> 23;

    return exponenent == 0xff && significand == 0; 
}

static inline int isDigitWithSign(char ch) {
    return (ch >= '0' && ch <= '9') || ch =='-' || ch == '+';
}

static inline int isDigit(char ch) {
    return (ch >= '0' && ch <= '9');
}

static int UIntegerDumps(unsigned int num, char *dst) {
    int count = 0;

    if(num == 0){
        dst[count++] = '0';
        return count;
    }
    
    char tmp[MAX_INT_LENGTH - 1];
    int i = 0;
    while(num > 0) {
        tmp[i++] = num % 10 + '0';
        num = num / 10;
    }

    for(int j = 0; j < i; ++j){
        dst[count + j] = tmp[i - j - 1];
    }
    count += i;

    return count;
}

static inline int IntegerDumps(int num, char *dst) {
    int count = 0;

    if(num < 0){
        dst[count++] = '-';
        num = -num;
    }

    return count + UIntegerDumps((unsigned int)num, dst+count);
}

static int ExtractInteger(char **src, int *res) {
    char *src_str = *src;
    int count = 0;
    int value = 0;
    int isNegative = 0;

    if(src_str[count] == '-'){
        isNegative = 1;
        count++;
    }
    else if(src_str[count] == '+')
        count++;
    
    while(isDigit(src_str[count])){
        value = value * 10 + src_str[count++] - '0';
    }
    *src += count;

    *res = isNegative ? -value : value;
    return count;
}

static int ExtractFloat(char **src, double *res) {
    char *src_str = *src;
    int count = 0, isNegative = 0, isFractional = 0;
    double value = 0, fraction;


    if(src_str[count] == '-') {
        isNegative = 1;
        count++;
    }
    else if(src_str[count] == '+') 
        count++;

    while(isDigit(src_str[count]) || src_str[count] == '.') {
        if(src_str[count] == '.'){
            if(isFractional)
                return value;
            isFractional = 1;
            fraction = 0.1f;
            count++;
        }

        else {
            if(!isFractional) {
                value = value * 10 + src_str[count++] - '0';
            }
            else {
                value += (src_str[count++] - '0') * fraction;
                fraction /= 10;
            }
        }
    }
    *src += count;

    *res = isNegative ? -value: value;
    return count;
}

static int NormalizeFloat(double *num) {
    const double positive_threashold = 1e7;
    const double negative_threashold = 1e-6;
    int exponent = 0;

    if(*num > positive_threashold) {
        if (*num >= 1e256) {
            *num /= 1e256;
            exponent += 256;
        }
        if (*num >= 1e128) {
            *num /= 1e128;
            exponent += 128;
        }
        if (*num >= 1e64) {
            *num /= 1e64;
            exponent += 64;
        }
        if (*num >= 1e32) {
            *num /= 1e32;
            exponent += 32;
        }
        if (*num >= 1e16) {
            *num /= 1e16;
            exponent += 16;
        }
        if (*num >= 1e8) {
            *num /= 1e8;
            exponent += 8;
        }
        if (*num >= 1e4) {
            *num /= 1e4;
            exponent += 4;
        }
        if (*num >= 1e2) {
            *num /= 1e2;
            exponent += 2;
        }
        if (*num >= 1e1) {
            *num /= 1e1;
            exponent += 1;
        }
    }
    if(*num > 0 && *num < negative_threashold) {
        if (*num <= 1e-255) {
            *num *= 1e256;
            exponent -= 256;
        }
        if (*num <= 1e-127) {
            *num *= 1e128;
            exponent -= 128;
        }
        if (*num <= 1e-63) {
            *num *= 1e64;
            exponent -= 64;
        }
        if (*num <= 1e-31) {
            *num *= 1e32;
            exponent -= 32;
        }
        if (*num <= 1e-15) {
            *num *= 1e16;
            exponent -= 16;
        }
        if (*num <= 1e-7) {
            *num *= 1e8;
            exponent -= 8;
        }
        if (*num <= 1e-3) {
            *num *= 1e4;
            exponent -= 4;
        }
        if (*num <= 1e-1) {
            *num *= 1e2;
            exponent -= 2;
        }
        if (*num <= 1e0) {
            *num *= 1e1;
            exponent -= 1;
        }
    }

    return exponent;
}

typedef struct float_result {
    unsigned int integral_part;
    unsigned int decimal_part;
    int exponent;
} float_result;

static float_result SplitFloat(double num) {
    float_result result = { 0 };

    result.exponent = NormalizeFloat(&num);
    result.integral_part = (unsigned int)num;

    double remainder = num - result.integral_part;
    remainder *= 1e6;
    result.decimal_part = (unsigned int)remainder;

    remainder -= result.decimal_part;
    if(remainder >= 0.5) {
        result.decimal_part++;
        if(result.decimal_part == 1000000){
            result.decimal_part = 0;
            result.integral_part++;
            if(result.exponent != 0 && result.integral_part >= 10){
                result.exponent++;
                result.integral_part = 1;
            }
        }
    }

    return result;
}

static int FloatDumps(double num, int decimals_to_print, char *dst) {
    int count = 0;
    
    if(num < 0) {
        dst[count++] = '-';
        num = -num;
    }

    if(num == 0) {
        dst[count++] = '0';
        return count;
    }

    if(IsNan(num)) {
        dst[count++] = 'n';
        dst[count++] = 'a';
        dst[count++] = 'n';
        return count;
    }

    if(IsInf(num)) {
        dst[count++] = 'i';
        dst[count++] = 'n';
        dst[count++] = 'f';
        return count;
    }

    float_result float_parts = SplitFloat(num);
    
    if(float_parts.decimal_part) {
        int width = 6;
        while(float_parts.decimal_part % 10 == 0 && width > 0){
            float_parts.decimal_part /= 10;
            width--;
        }

        // width = 6
        // decimal_part = 544447
        // decimals_to_print = 1  => divide by 100000
        // remainder = 5.44447
        // decimal_part = 5
        // remainder = 0.44447

        if(decimals_to_print >= 0){
            if(width > decimals_to_print){
                double remainder = float_parts.decimal_part / _pow_of_10[width - decimals_to_print];
                float_parts.decimal_part = (unsigned int)remainder;
                remainder -= float_parts.decimal_part;

                if(remainder >= _round_threashold[width - decimals_to_print]){
                    float_parts.decimal_part++;
                    if(float_parts.decimal_part >= _pow_of_10[decimals_to_print]) {
                        float_parts.integral_part++;
                        float_parts.decimal_part = 0;
                    }
                }
            }
        }


    }


    count += UIntegerDumps(float_parts.integral_part, dst + count);
    if(decimals_to_print >= 0){
        dst[count++] = '.';
        int decimals_printed = UIntegerDumps(float_parts.decimal_part, dst + count);
        count += decimals_printed;

        if(decimals_printed < decimals_to_print) {
            for(int i = 0; i < decimals_to_print - decimals_printed; ++i) {
                dst[count++] = '0';
            }
        }
    }
    else if(float_parts.decimal_part) {
        dst[count++] = '.';
        int decimals_printed = UIntegerDumps(float_parts.decimal_part, dst + count);
        count += decimals_printed;
    }

    if(float_parts.exponent) {
        dst[count++] = 'e';
        count += IntegerDumps(float_parts.exponent, dst + count);        
    }    

    return count;
}