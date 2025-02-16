# otls-bench

## Patch Code

```bash
bash ./patch_code.sh
```

## Compile

### Native on x86


```bash
bash ./build_native_x86.sh
```

### Wasm on x86



```bash
bash ./build_wasm_x86.sh
```


### Native on arm


```bash
bash ./build_native_arm.sh
```

## Run


### Verifier
```bash
sudo ./bench.sh verifier 0.0.0.0 12345 lo [native|wasm]
```

### Prover

```bash
sudo ./bench.sh prover 127.0.0.1 12345 lo [native|wasm]
```
