# UART Console (Picolibc + FSP) – User Manual

## 1. Overview

This module provides a **UART-backed stdio interface** (`stdin`, `stdout`, `stderr`) for Renesas FSP-based embedded systems using **picolibc**.

It enables:

* `printf()` → UART TX
* `scanf()/getchar()` → UART RX
* Buffered, interrupt-driven communication via **ByteQ**

***

## 2. Key Features

* Fully integrates with **picolibc `FILE` streams**
* Non-blocking interrupt-driven UART TX/RX
* Byte queue buffering using `r_byteq`
* Automatic initialization on first use
* Line-buffered output (`'\n'` triggers flush)

***

## 3. Dependencies

### Mandatory Components

| Component   | Requirement                                   |
| ----------- | --------------------------------------------- |
| Toolchain   | LLVM ARM Embedded Toolchain with **picolibc** |
| FSP Modules | `r_uart`, `r_byteq`                           |
| BSP         | Any supported RA device                       |

***

### Compile-Time Requirement

```c
#if !defined (__PICOLIBC__)
#error "Use this file only with picolibc. Typically packaged with LLVM ARM Embedded Toolchain."
#endif 
```

***

## 4. FSP Configuration

### 4.1 Add UART Instance

In FSP configurator:

* Add a UART instance (e.g., SCI UART)
* Name it:

```
g_uart_jlob_vcom
```

***

### 4.2 Configure UART Parameters

Recommended settings:

| Parameter | Value             |
| --------- | ----------------- |
| Mode      | Asynchronous UART |
| Baud Rate | 115200 (typical)  |
| Data Bits | 8                 |
| Parity    | None              |
| Stop Bits | 1                 |

***

### 4.3 Callback Configuration

Ensure one of the following:

#### Option A (Preferred)

Leave callback **unset** — module assigns it dynamically.

#### Option B

Set callback to:

```c
uart_jlob_vcom_callback
```

***

### 4.4 Enable ByteQ

Add two ByteQ instances **implicitly used internally**:

* No external configuration required
* Managed by this module

***

## 5. Integration Steps

### Step 1: Add File to Project

Add only one of the available source file to your project:

```
syscalls_picolib_stdio_to_uart.c
```

***

### Step 2: Ensure UART Instance Exists

In your BSP:

```c
extern uart_instance_t g_uart_jlob_vcom;
```

***

### Step 3: No Explicit Init Required

Initialization is handled automatically:

```c
jlob_uart_init_once()
```

Triggered on first:

* `printf()`
* `getchar()`
* `fflush()`

***

## 6. Usage

### 6.1 Writing to Console

```c
printf("Hello UART\n");
fprintf(stderr, "Error message\n");
```

Behavior:

* Characters buffered in TX queue
* Flush occurs:
  * Automatically on `'\n'`
  * On buffer full
  * On explicit `fflush(stdout)`

***

### 6.2 Reading from Console

```c
int c = getchar();
```

Behavior:

* Blocks until character is received
* Data sourced from RX queue (ISR-filled)

***

### 6.3 Flushing Output

```c
fflush(stdout);
```

Triggers:

* Queue → UART transmission
* Non-blocking TX start
* Completion handled via ISR

***

## 7. Data Flow Architecture

### TX Flow

```
printf()
   ↓
uart_putc()
   ↓
ByteQ (TX queue)
   ↓
uart_flush()
   ↓
malloc buffer
   ↓
UART write()
   ↓
ISR → free buffer
```

***

### RX Flow

```
UART ISR (RX event)
   ↓
ByteQ (RX queue)
   ↓
uart_getc()
   ↓
getchar()
```

***

## 8. Callback Behavior

### UART Events Handled

| Event                      | Action                             |
| -------------------------- | ---------------------------------- |
| `UART_EVENT_RX_CHAR`       | Push character into RX queue       |
| `UART_EVENT_TX_DATA_EMPTY` | Release TX buffer, clear busy flag |

***

### Special Handling

* Backspace (`'\b'`) removes last RX character:

```c
R_BYTEQ_PopLast()
```

***

## 9. Configuration Parameters

### Buffer Sizes

```c
static uint8_t rx_memory_buffer[256];
static uint8_t tx_memory_buffer[256];
```

You may tune:

| Parameter | Recommendation         |
| --------- | ---------------------- |
| RX Buffer | ≥ expected line length |
| TX Buffer | ≥ max print burst      |

***

## 10. Error Handling

### Return Conventions

| Function        | Return                  |
| --------------- | ----------------------- |
| `uart_putc`  | Written character or -1 |
| `uart_getc`  | Character or -1         |
| `uart_flush` | 0 or -1                 |

***

### Common Failure Conditions

| Condition     | Cause                        |
| ------------- | ---------------------------- |
| `-1` on putc  | Queue full + flush failure   |
| `-1` on getc  | UART init failure            |
| `-1` on flush | malloc or UART write failure |

***

## 11. Limitations

### 11.1 Blocking Behavior

* RX is **blocking**
* TX flush waits on `tx_busy`

***

### 11.2 Heap Usage

* Uses `malloc()` per flush
* Not deterministic in RT systems

***

### 11.3 No Timeout Support

* Infinite wait loops exist:
  * RX wait
  * TX busy

***

### 11.4 Single UART Instance

* Hardcoded to:

```
g_uart_jlob_vcom
```

***

## 12. Recommended Best Practices

### For Production Systems

* Replace `malloc()` with static buffer
* Add timeout to blocking loops
* Protect shared variables (ISR vs thread)
* Increase buffer sizes for high throughput

***

### For Debug Use

* Keep default configuration
* Works well for:
  * Logging
  * CLI interaction
  * Basic monitoring

***

## 13. Example

### Minimal Usage

```c
int main(void)
{
    printf("System Init\n");

    while (1)
    {
        int c = getchar();
        printf("You typed: %c\n", c);
    }
}
```

***

## 14. Troubleshooting

| Issue              | Check                 |
| ------------------ | --------------------- |
| No output          | UART pins / baud rate |
| Stuck on getchar() | No incoming data      |
| Crash on flush     | malloc failure        |
| Missing interrupts | NVIC config for UART  |

***

## 15. Summary

This module provides a **clean stdio abstraction** over FSP UART using:

* ByteQ buffering
* ISR-driven communication
* Minimal integration effort

It is best suited for:

* Debug console
* Development phase instrumentation

For production-grade systems, consider:

* Removing heap usage
* Adding non-blocking APIs
* RTOS integration

***
