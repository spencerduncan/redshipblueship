#ifndef PR_OS_HOST_H
#define PR_OS_HOST_H

void MM___osInitialize_common(void);
void MM___osInitialize_autodetect(void);

#define osInitialize() MM___osInitialize_common()

#endif
