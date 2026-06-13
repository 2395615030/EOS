#include "emotionos/memory/mnemonic.h"
int main() { 
    eos::memory::MnemonicOrgan mem;
    float v[128]={}; float h[16]={}; float e[8]={};
    int idx = mem.write(v, h, e, 0.0f, 0);
    auto r = mem.recall(v, h);
    mem.debug_print();
    return 0; 
}
