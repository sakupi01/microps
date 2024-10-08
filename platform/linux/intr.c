#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include "platform.h"
#include "util.h"

static sigset_t sigmask;
// 割り込み要求の構造体
struct irq_entry {
    struct irq_entry *next;
    unsigned int irq;
    int (*handler)(unsigned int irq, void *dev);
    int flags;
    char name[16];
    void *dev;
};

/* NOTE: if you want to add/delete the entries after intr_run(), you need to protect these lists with a mutex. */
static struct irq_entry *irqs;
static sigset_t sigmask;

static pthread_t tid;
static pthread_barrier_t barrier;

// 割り込み要求処理
int
intr_request_irq(unsigned int irq, int (*handler)(unsigned int irq, void *dev), int flags, const char *name, void *dev)
{
    struct irq_entry *entry;

    debugf("irq=%u, flags=%d, name=%s", irq, flags, name);
    for (entry = irqs; entry; entry = entry->next) {
        if (entry->irq == irq) {
            if (entry->flags ^ INTR_IRQ_SHARED || flags ^ INTR_IRQ_SHARED) {
                errorf("conflicts with already registered IRQs");
                return -1;
            }
        }
    }
    entry = memory_alloc(sizeof(*entry));
    if (!entry) {
        errorf("memory_alloc() failure");
        return -1;
    }
    entry->irq = irq;
    entry->handler = handler;
    entry->flags = flags;
    strncpy(entry->name, name, sizeof(entry->name)-1);
    entry->dev = dev;
    entry->next = irqs;
    irqs = entry;
    sigaddset(&sigmask, irq);
    debugf("registered: irq=%u, name=%s", irq, name);

    return 0;
}


// 割り込みの初期化
int
intr_init(void)
{
    tid = pthread_self();
    pthread_barrier_init(&barrier, NULL, 2);
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGHUP);
    return 0;
}


// 割り込み処理が実行できるようになるまで待機＆割り込み処理スレッドを作成
int
intr_run(void)
{
    int err;

    err = pthread_sigmask(SIG_BLOCK, &sigmask, NULL);
    if (err) {
        errorf("pthread_sigmask() %s", strerror(err));
        return -1;
    }
    err = pthread_create(&tid, NULL, intr_thread, NULL);
    if (err) {
        errorf("pthread_create() %s", strerror(err));
        return -1;
    }
    pthread_barrier_wait(&barrier);
    return 0;
}

// 割り込み処理スレッドにシグナルを送信して終わるのを待つ
void
intr_shutdown(void)
{
    if (pthread_equal(tid, pthread_self()) != 0) {
        /* Thread not created. */
        return;
    }
    pthread_kill(tid, SIGHUP);
    pthread_join(tid, NULL);
}

// 割り込みスレッドのエントリポイント
static void *
intr_thread(void *arg)
{
    int terminate = 0, sig, err;
    struct irq_entry *entry;

    debugf("start...");
    pthread_barrier_wait(&barrier);
    while (!terminate) {
        err = sigwait(&sigmask, &sig);
        if (err) {
            errorf("sigwait() %s", strerror(err));
            break;
        }
        switch (sig) {
        case SIGHUP:
            terminate = 1;
            break;
        default:
            for (entry = irqs; entry; entry = entry->next) {
                if (entry->irq == (unsigned int)sig) {
                    debugf("irq=%d, name=%s", entry->irq, entry->name);
                    entry->handler(entry->irq, entry->dev);
                }
            }
            break;
        }
    }
    debugf("terminated");
    return NULL;
}

static pthread_t tid;
// 割り込み処理の発生
int
intr_raise_irq(unsigned int irq)
{
    return pthread_kill(tid, (int)irq);
}
