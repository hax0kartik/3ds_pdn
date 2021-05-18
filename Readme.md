# 3ds_pdn
Open source implementation of `PDN` sysmodule.

## Details
The stock PDN module is very small, only about ~100 funcs, and was very easy to reverse.

All the module does is handle writing to various PDN registers.

The generated executable is only `0xb98` in size while stock PDN is `0x1d84` bytes large, roughly saving us `4500` bytes. (Thanks! @luigoalma)

## Build
To build you need to have ctrtool in your path after which you can just do `make`.

## Credits

@luigoalma:- Helping me imporve the code