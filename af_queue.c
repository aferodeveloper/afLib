/*
 * af_queue.c
 *
 *  Created on: Apr 27, 2015
 *      Author: chrisatkiban
 */

#include <stddef.h>
#include <stdbool.h>
#include "af_queue.h"
#include "af_logger.h"

static uint8_t (*m_p_preemption_disable)(void);
static void (*m_p_preemption_enable)(uint8_t is_nested);

static void __af_queue_put(queue_t *p_q, af_queue_elem_desc_t *p_desc)
{
    if (p_q->p_head == NULL) {
        p_q->p_tail = p_q->p_head = p_desc;
    } else {
        p_q->p_tail->p_next_alloc = p_desc;
        p_q->p_tail = p_desc;
    }
}

static void _af_queue_put(queue_t *p_q, af_queue_elem_desc_t *p_desc, bool interrupt_context)
{
    if (!interrupt_context) {
        uint8_t is_nested;

        is_nested = m_p_preemption_disable();
        {
            __af_queue_put( p_q, p_desc );
        }
        m_p_preemption_enable(is_nested);
    } else {
        __af_queue_put( p_q, p_desc );
    }
}

static af_queue_elem_desc_t *__af_queue_elem_alloc(queue_t *p_q)
{
    af_queue_elem_desc_t *p_desc = NULL;

    if (p_q->p_free_head != NULL) {
        p_desc = p_q->p_free_head;
        p_q->p_free_head = p_desc->p_next_free;
        p_desc->p_next_alloc = NULL;
    }

    return p_desc;
}

static void *_af_queue_elem_alloc(queue_t *p_q, bool interrupt_context)
{
    af_queue_elem_desc_t *p_desc;

    if (!interrupt_context) {
        uint8_t is_nested;

        is_nested = m_p_preemption_disable();
        {
            p_desc = __af_queue_elem_alloc(p_q);
        }
        m_p_preemption_enable(is_nested);
    } else {
        p_desc = __af_queue_elem_alloc(p_q);
    }

    return p_desc ? p_desc->data : NULL;
}

static af_queue_elem_desc_t *__af_queue_get(queue_t *p_q)
{
    af_queue_elem_desc_t *p_desc = p_q->p_head;

    if (p_desc != NULL) {
        p_q->p_head = p_desc->p_next_alloc;
        p_desc->p_next_alloc = NULL;
    }

    if (p_q->p_head == NULL) {
        p_q->p_tail = NULL;
    }

    return p_desc;
}

static void *_af_queue_get(queue_t *p_q, bool interrupt_context)
{
    af_queue_elem_desc_t *p_desc;

    if (!interrupt_context) {
        uint8_t is_nested;

        is_nested = m_p_preemption_disable();
        {
            p_desc = __af_queue_get(p_q);
        }
        m_p_preemption_enable(is_nested);
    } else {
        p_desc = __af_queue_get(p_q);
    }

    return p_desc ? p_desc->data : NULL;
}

static void __af_queue_elem_free(queue_t *p_q, void *p_data)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    af_queue_elem_desc_t *p_desc = (af_queue_elem_desc_t *)((uint8_t *)p_data - __builtin_offsetof(struct af_queue_elem_desc_s, data));
#pragma GCC diagnostic push
    af_queue_elem_desc_t *p_tmp_desc;

    p_tmp_desc = p_q->p_free_head;
    p_q->p_free_head = p_desc;
    p_desc->p_next_free = p_tmp_desc;
}

static void _af_queue_elem_free(queue_t *p_q, void *p_data, bool interrupt_context)
{
    if (!interrupt_context) {
        uint8_t is_nested;

        is_nested = m_p_preemption_disable();
        {
            __af_queue_elem_free(p_q, p_data);
        }
        m_p_preemption_enable(is_nested);
    } else {
        __af_queue_elem_free(p_q, p_data);
    }
}

static void *_af_queue_peek(queue_t *p_q, bool interrupt_context)
{
    return p_q->p_head ? p_q->p_head->data : NULL;
}

static void *_af_queue_peek_tail(queue_t *p_q, bool interrupt_context)
{
    return p_q->p_tail ? p_q->p_tail->data : NULL;
}

void af_queue_put(queue_t *p_q, void *p_data)
{
    af_queue_elem_desc_t *p_desc = (af_queue_elem_desc_t *)((uint8_t *)p_data - __builtin_offsetof(struct af_queue_elem_desc_s, data));
    _af_queue_put(p_q, p_desc, false);
}

void af_queue_put_from_interrupt(queue_t *p_q, void *p_data)
{
    af_queue_elem_desc_t *p_desc = (af_queue_elem_desc_t *)((uint8_t *)p_data - __builtin_offsetof(struct af_queue_elem_desc_s, data));
    _af_queue_put(p_q, p_desc, true);
}

void *af_queue_elem_alloc(queue_t *p_q)
{
    return _af_queue_elem_alloc(p_q, false);
}

void *af_queue_elem_alloc_from_interrupt(queue_t *p_q)
{
    return _af_queue_elem_alloc(p_q, true);
}

void *af_queue_get(queue_t *p_q)
{
    return _af_queue_get(p_q, false);
}

void *af_queue_get_from_interrupt(queue_t *p_q)
{
    return _af_queue_get(p_q, true);
}

void *af_queue_peek(queue_t *p_q)
{
    return _af_queue_peek(p_q, false);
}

void *af_queue_peek_from_interrupt(queue_t *p_q)
{
    return _af_queue_peek(p_q, true);
}

void *af_queue_peek_tail(queue_t *p_q)
{
    return _af_queue_peek_tail(p_q, false);
}

void *af_queue_peek_tail_from_interrupt(queue_t *p_q)
{
    return _af_queue_peek_tail(p_q, true);
}

void af_queue_elem_free(queue_t *p_q, void *p_data)
{
    _af_queue_elem_free(p_q, p_data, false);
}

void af_queue_elem_free_from_interrupt(queue_t *p_q, void *p_data)
{
    _af_queue_elem_free(p_q, p_data, true);
}

void af_queue_init(queue_t *p_q, int elem_size, int max_elem, uint8_t *p_mem)
{
    af_queue_elem_desc_t *p_desc;
    af_queue_elem_desc_t *p_desc_next;
    int offset;
    int i = 0;

    p_q->p_head = NULL;
    p_q->p_tail = NULL;

    // string all elements together and onto the null-terminated free list to start
    p_q->p_free_head = (af_queue_elem_desc_t *)p_mem;

    for (i = 0; i < max_elem - 1; ++i) {
        offset = i * (ALIGN_SIZE(sizeof(af_queue_elem_desc_t), 4) + ALIGN_SIZE(elem_size, 4));
        p_desc = (af_queue_elem_desc_t *)(p_mem + offset);

        offset = (i + 1) * (ALIGN_SIZE(sizeof(af_queue_elem_desc_t), 4) + ALIGN_SIZE(elem_size, 4));
        p_desc_next = (af_queue_elem_desc_t *)(p_mem + offset);
        p_desc->p_next_free = p_desc_next;
    }

    offset = (max_elem - 1) * (ALIGN_SIZE(sizeof(af_queue_elem_desc_t), 4) + ALIGN_SIZE(elem_size, 4));
    p_desc = (af_queue_elem_desc_t *)(p_mem + offset);
    p_desc->p_next_free = NULL;
}

void af_queue_init_system(uint8_t (*p_preemption_disable)(void), void (*p_preemption_enable)(uint8_t is_nested))
{
    m_p_preemption_disable = p_preemption_disable;
    m_p_preemption_enable = p_preemption_enable;
}

void af_queue_dump(queue_t *p_q)
{
    af_queue_elem_desc_t *p_elem;

    af_logger_print_buffer("Q ");
    af_logger_print_formatted_value((int)p_q, AF_LOGGER_HEX);
    af_logger_print_buffer(" free_head ");
    af_logger_print_formatted_value((int)p_q->p_free_head, AF_LOGGER_HEX);
    af_logger_print_buffer(" head ");
    af_logger_print_formatted_value((int)p_q->p_head, AF_LOGGER_HEX);
    af_logger_print_buffer(" tail ");
    af_logger_println_formatted_value((int)p_q->p_tail, AF_LOGGER_HEX);

    af_logger_println_buffer("In Queue: "); // Not all allocated are in queue (until af_queue_put)
    p_elem = p_q->p_head;
    while (p_elem) {
        af_logger_println_formatted_value((int)p_elem, AF_LOGGER_HEX);
        p_elem = p_elem->p_next_alloc;
    }

    af_logger_println_buffer("Free:");
    p_elem = p_q->p_free_head;
    while (p_elem) {
        af_logger_println_formatted_value((int)p_elem, AF_LOGGER_HEX);
        p_elem = p_elem->p_next_free;
    }
}
