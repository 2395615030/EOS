#include "emotionos/memory/stg.h"
int main() { 
    eos::memory::STG stg; 
    float v[128]={}; 
    float h[16]={}; 
    float e[8]={}; 
    int idx = stg.write(v, h, e, 0.0f, 0); 
    int out_i[8]; float out_s[8]; 
    int n = stg.recall(v, h, out_i, out_s, 8); 
    return 0; 
}
