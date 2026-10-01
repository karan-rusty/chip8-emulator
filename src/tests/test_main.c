#include "test.h"

int failures;
int checks;

int main(void)
{
    test_init();
    test_font();
    test_font_pointer();
    test_font_draw();
    test_load_rom();
    test_fetch();
    test_jump();
    test_call_return();
    test_stack_saturates();
    test_load_add();
    test_skips();
    test_load_i();
    test_jump_offset();
    test_clear();
    test_sys_is_ignored();
    test_alu();
    test_alu_flags();
    test_alu_vf_order();
    test_logical_clears_vf();
    test_display_wait();
    test_random();
    test_draw();
    test_draw_row_count_is_a_nibble();
    test_draw_wraps();
    test_draw_clips();
    test_draw_sets_collision();
    test_schip_draw_forms();
    test_key_skips();
    test_wait_key();
    test_timers();
    test_add_to_i();
    test_bcd();
    test_mem_move();
    test_unknown_f_opcode_is_inert();
    test_unknown_opcode_is_inert();
    test_pc_wraps();

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
