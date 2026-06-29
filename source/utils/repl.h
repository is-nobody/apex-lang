#ifndef REPL_H
#define REPL_H

#ifdef __cplusplus
extern "C" {
#endif

// runs the interactive read-eval-print loop with raw terminal input
void repl_run(void);

#ifdef __cplusplus
}
#endif

#endif // REPL_H