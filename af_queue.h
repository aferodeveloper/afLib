/*
 * q.h
 *
 *  Created on: Apr 27, 2015
 *      Author: chrisatkiban
 */

#ifndef AF_QUEUE_H
#define AF_QUEUE_H

#include <stdint.h>

#ifdef  __cplusplus
extern "C" {
#endif

#define ALIGN_SIZE( sizeToAlign, PowerOfTwo ) \
        (((sizeToAlign) + (PowerOfTwo) - 1) & ~((PowerOfTwo) - 1))

#define AF_QUEUE_DECLARE(q, elem_size, max_elem) queue_t volatile (q); uint8_t volatile (q##_mem)[(max_elem) * (ALIGN_SIZE(sizeof(af_queue_elem_desc_t), 4) + ALIGN_SIZE((elem_size), 4))]
#define AF_QUEUE_INIT(q, elem_size, max_elem) af_queue_init((queue_t *)&(q), elem_size, max_elem, (uint8_t *)(q##_mem))
#define AF_QUEUE_GET(p_q) af_queue_get((queue_t *)(p_q))
#define AF_QUEUE_GET_FROM_INTERRUPT(p_q) af_queue_get_from_interrupt((queue_t *)(p_q))
#define AF_QUEUE_ELEM_ALLOC(p_q) af_queue_elem_alloc((queue_t *)(p_q))
#define AF_QUEUE_ELEM_ALLOC_FROM_INTERRUPT(p_q) af_queue_elem_alloc_from_interrupt((queue_t *)(p_q))
#define AF_QUEUE_ELEM_FREE(p_q, p_data) af_queue_elem_free((queue_t *)(p_q), p_data)
#define AF_QUEUE_ELEM_FREE_FROM_INTERRUPT(p_q, p_data) af_queue_elem_free_from_interrupt((queue_t *)(p_q), p_data)
#define AF_QUEUE_PEEK(p_q) af_queue_peek((queue_t *)(p_q))
#define AF_QUEUE_PEEK_FROM_INTERRUPT(p_q) af_queue_peek_from_interrupt((queue_t *)(p_q))
#define AF_QUEUE_PEEK_TAIL(p_q) af_queue_peek_tail((queue_t *)(p_q))
#define AF_QUEUE_PEEK_TAIL_FROM_INTERRUPT(p_q) af_queue_peek_tail_from_interrupt((queue_t *)(p_q))
#define AF_QUEUE_PUT(p_q, p_data) af_queue_put((queue_t *)(p_q), p_data)
#define AF_QUEUE_PUT_FROM_INTERRUPT(p_q, p_data) af_queue_put_from_interrupt((queue_t *)(p_q), p_data)
#define AF_QUEUE_GET_NUM_AVAILABLE(p_q) af_queue_get_num_available((queue_t *)(p_q))

typedef struct af_queue_elem_desc_s
{
    struct af_queue_elem_desc_s *p_next_alloc;
    struct af_queue_elem_desc_s *p_next_free;
    uint8_t data[1];
} af_queue_elem_desc_t;

typedef struct queue_s
{
    af_queue_elem_desc_t *p_head;
    af_queue_elem_desc_t *p_tail;
    af_queue_elem_desc_t *p_free_head;
    uint32_t num_available;
} queue_t;

void af_queue_init_system(uint8_t (*p_preemption_disable)(void), void (*p_preemption_enable)(uint8_t is_nested));
void af_queue_init(queue_t *p_q, int elem_size, int max_elem, uint8_t *p_mem);
void *af_queue_peek(queue_t *p_q);
void *af_queue_peek_from_interrupt(queue_t *p_q);
void *af_queue_peek_tail(queue_t *p_q);
void *af_queue_peek_tail_from_interrupt(queue_t *p_q);
void *af_queue_get(queue_t *p_q);
void *af_queue_get_from_interrupt(queue_t *p_q);
void *af_queue_elem_alloc(queue_t *p_q);
void *af_queue_elem_alloc_from_interrupt(queue_t *p_q);
void af_queue_elem_free(queue_t *p_q, void *p_data);
void af_queue_elem_free_from_interrupt(queue_t *p_q, void *p_data);
void af_queue_put(queue_t *p_q, void *p_data);
void af_queue_put_from_interrupt(queue_t *p_q, void *p_data);
void af_queue_dump(queue_t *p_q, void (*p_element_data)(void*));
uint32_t af_queue_get_num_available(queue_t *p_q);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* AF_QUEUE_H */
