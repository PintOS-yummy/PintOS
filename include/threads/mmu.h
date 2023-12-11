#ifndef THREAD_MMU_H
#define THREAD_MMU_H

#include <stdbool.h>
#include <stdint.h>
#include "threads/pte.h"

/*
include/threads/mmu.h 은 페이지 테이블 상에서의 활동을 제공

PML4 = Page-Map-Level-4

pml4_for_each의 인자로 전달될 수 있는 func의 예시
static bool
stat_page (uint64_t *pte, void *va,  void *aux) {
        if (is_user_vaddr (va))
                printf ("user page: %llx\n", va);
        if (is_writable (va))
                printf ("writable page: %llx\n", va);
        return true;
}
*/

/*
* 각 pml4가 유효한 entry를 가지고 있는지 검사하며, 
* 검사를 위해 보조값 aux를 받는 함수 func를 추가적으로 활용합니다. 
* va는 entry의 가상주소입니다.(pte == page table entry == 여기서 말하는 entry)
* pte_for_each_func가 false를 리턴하면, 반복을 멈추고 false를 리턴합니다. */
typedef bool pte_for_each_func (uint64_t *pte, void *va, void *aux);
bool pml4_for_each (uint64_t *, pte_for_each_func *, void *);

uint64_t *pml4e_walk (uint64_t *pml4, const uint64_t va, int create);
uint64_t *pml4_create (void);
void pml4_destroy (uint64_t *pml4);
void pml4_activate (uint64_t *pml4);
void *pml4_get_page (uint64_t *pml4, const void *upage);
bool pml4_set_page (uint64_t *pml4, void *upage, void *kpage, bool rw);
void pml4_clear_page (uint64_t *pml4, void *upage);
bool pml4_is_dirty (uint64_t *pml4, const void *upage);
void pml4_set_dirty (uint64_t *pml4, const void *upage, bool dirty);
bool pml4_is_accessed (uint64_t *pml4, const void *upage);
void pml4_set_accessed (uint64_t *pml4, const void *upage, bool accessed);

// PTE가 가리키는 가상주소가 작성 가능한지 여부 확인
#define is_writable(pte) (*(pte) & PTE_W)

// 페이지 테이블 엔트리(PTE)의 주인이 유저인지 커널인지 확인
#define is_user_pte(pte) (*(pte) & PTE_U) 
#define is_kern_pte(pte) (!is_user_pte (pte))

#define pte_get_paddr(pte) (pg_round_down(*(pte)))

/* Segment descriptors for x86-64. */
struct desc_ptr {
	uint16_t size;
	uint64_t address;
} __attribute__((packed));

#endif /* thread/mm.h */
