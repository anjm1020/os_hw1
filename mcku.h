#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct pcb {
    char pid;
    FILE *fd;
    char *pgtable;
};

struct pcblist_entry {
    struct pcb *data;
    struct pcblist_entry *next;
};

const int PTE_LEN = 8;
const int NUMBER_OF_PTE = 16;
const int NUMBER_OF_PAGE_FRAME = 64;

extern struct pcb *current;
extern char *ptbr;

struct pcblist_entry *plist_head;
char *freeList;

void ku_scheduler(char pid) {
    struct pcblist_entry *next, *tail, *tmp;

    if (!current->pgtable) free(current);

    if (!plist_head) {
        current = NULL;
        ptbr = NULL;
        return;
    }

    if (plist_head->next && plist_head->data->pid == pid) {
        tmp = plist_head;
        plist_head = plist_head->next;
        tail = tmp;
        while (tail->next != NULL) tail = tail->next;
        tmp->next = NULL;
        tail->next = tmp;
    }

    next = tail = plist_head;
    while (tail->next != NULL) tail = tail->next;
    tail->next = plist_head;
    tmp = plist_head;
    plist_head = plist_head->next;
    tmp->next = NULL;

    if (!next->data) {
        current = NULL;
        ptbr = NULL;
    } else {
        current = next->data;
        ptbr = next->data->pgtable;
    }
}

void ku_pgfault_handler(char va) {
    int pfn, vpn;
    char pte;

    pfn = -1;
    for (int i = 0; i < NUMBER_OF_PAGE_FRAME; i++) {
        if (!freeList[i]) {
            pfn = i;
            freeList[i] = 1;
            break;
        }
    }

    if (pfn == -1) return;

    vpn = (va & 0xF0) >> 4;

    pte = (pfn << 2) | 0x01;

    ptbr[vpn] = pte;
}

void ku_proc_exit(char pid) {
    struct pcb *deleted;
    struct pcblist_entry *curr = plist_head, *prev = NULL, *tail;
    char *pgtb;
    int pfn;

    while (curr->data->pid != pid) {
        prev = curr;
        curr = curr->next;
    }

    deleted = curr->data;
    if (prev)
        prev->next = NULL;
    else {
        plist_head = plist_head->next;
    }

    pgtb = deleted->pgtable;
    for (int i = 0; i < NUMBER_OF_PTE; i++) {
        if (pgtb[i]) {
            pfn = (unsigned char)(pgtb[i]) >> 2;
            freeList[pfn] = 0;
        }
    }

    curr->data = NULL;
    free(deleted->pgtable);
    fclose(deleted->fd);
    free(deleted);
    free(curr);
    current = malloc(sizeof(struct pcb));
    current->pid = pid;
    current->pgtable = NULL;
}

void ku_proc_init(int nprocs, char *flist) {
    char pte, *pgtb, line[1024], pid;
    struct pcb *newPcb;
    struct pcblist_entry *curr;
    FILE *proc_fd;

    FILE *fd = fopen(flist, "r");

    if (fd == NULL) exit(1);

    freeList = malloc(sizeof(char) * NUMBER_OF_PAGE_FRAME);
    for (int i = 0; i < NUMBER_OF_PAGE_FRAME; i++)
        freeList[i] = 0;

    pid = 0;
    plist_head = malloc(sizeof(struct pcblist_entry));
    plist_head->data = NULL;
    plist_head->next = NULL;
    for (int i = 0; i < nprocs; i++) {
        if (fgets(line, 1024, fd) == NULL) exit(1);
        proc_fd = fopen(line, "r");
        pte = 0 << (PTE_LEN - 1);
        pgtb = malloc(sizeof(pte) * NUMBER_OF_PTE);

        for (int i = 0; i < NUMBER_OF_PTE; i++)
            pgtb[i] = pte;
        newPcb = malloc(sizeof(struct pcb));
        newPcb->pid = pid++;
        newPcb->fd = proc_fd;
        newPcb->pgtable = pgtb;

        curr = plist_head;
        if (!curr->data) {
            curr->data = newPcb;
            continue;
        }

        while (curr->next) curr = curr->next;
        curr->next = malloc(sizeof(struct pcblist_entry));
        curr->next->data = newPcb;
        curr->next->next = NULL;
    }
    // while (fgets(line, 1024, fd) != NULL) {
    //     if (line[strlen(line) - 1] == '\n') line[strlen(line) - 1] = '\0';

    //     proc_fd = fopen(line, "r");
    //     pte = 0 << (PTE_LEN - 1);
    //     pgtb = malloc(sizeof(pte) * NUMBER_OF_PTE);

    //     for (int i = 0; i < NUMBER_OF_PTE; i++)
    //         pgtb[i] = pte;
    //     newPcb = malloc(sizeof(struct pcb));
    //     newPcb->pid = pid++;
    //     newPcb->fd = proc_fd;
    //     newPcb->pgtable = pgtb;

    //     curr = plist_head;
    //     if (!curr->data) {
    //         curr->data = newPcb;
    //         continue;
    //     }

    //     while (curr->next) curr = curr->next;
    //     curr->next = malloc(sizeof(struct pcblist_entry));
    //     curr->next->data = newPcb;
    //     curr->next->next = NULL;
    // }

    current = plist_head->data;
    ptbr = current->pgtable;
}
