/* Subroutines for insn-output.c for Motorola 88000.
   Copyright (C) 1988, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001, 2002 Free Software Foundation, Inc. 
   Contributed by Michael Tiemann (tiemann@mcc.com)
   Currently maintained by (gcc@dg-rtp.dg.com)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"
#include "conditions.h"
#include "output.h"
#include "insn-attr.h"
#include "tree.h"
#include "function.h"
#include "expr.h"
#include "libfuncs.h"
#include "c-tree.h"
#include "flags.h"
#include "recog.h"
#include "toplev.h"
#include "tm_p.h"
#include "target.h"
#include "target-def.h"
#include "tree-gimple.h"

#ifdef REGISTER_PREFIX
const char *m88k_register_prefix = REGISTER_PREFIX;
#else
const char *m88k_register_prefix = "";
#endif
char m88k_volatile_code;

int m88k_fp_offset	= 0;	/* offset of frame pointer if used */
int m88k_stack_size	= 0;	/* size of allocated stack (including frame) */
int m88k_case_index;

rtx m88k_compare_reg;		/* cmp output pseudo register */
rtx m88k_compare_op0;		/* cmpsi operand 0 */
rtx m88k_compare_op1;		/* cmpsi operand 1 */

enum processor_type m88k_cpu;	/* target cpu */

static void m88k_maybe_dead (rtx);
static void m88k_output_function_epilogue (FILE *, HOST_WIDE_INT);
static rtx m88k_struct_value_rtx (tree, int);
static int m88k_adjust_cost (rtx, rtx, rtx, int);
static bool m88k_handle_option (size_t, const char *, int);
static bool m88k_pass_by_reference (CUMULATIVE_ARGS *, enum machine_mode,
				    tree, bool);
static bool m88k_return_in_memory (tree, tree);
static void m88k_setup_incoming_varargs (CUMULATIVE_ARGS *, enum machine_mode,
					 tree, int *, int);
static tree m88k_build_va_list (void);
static tree m88k_gimplify_va_arg (tree, tree, tree *, tree *);
static bool m88k_rtx_costs (rtx, int, int, int *);
static int m88k_address_cost (rtx);
static void m88k_output_file_start (void);

/* Initialize the GCC target structure.  */
#if !defined(OBJECT_FORMAT_ELF)
#undef TARGET_ASM_BYTE_OP
#define TARGET_ASM_BYTE_OP "\tbyte\t"
#undef TARGET_ASM_ALIGNED_HI_OP
#define TARGET_ASM_ALIGNED_HI_OP "\thalf\t"
#undef TARGET_ASM_ALIGNED_SI_OP
#define TARGET_ASM_ALIGNED_SI_OP "\tword\t"
#undef TARGET_ASM_UNALIGNED_HI_OP
#define TARGET_ASM_UNALIGNED_HI_OP "\tuahalf\t"
#undef TARGET_ASM_UNALIGNED_SI_OP
#define TARGET_ASM_UNALIGNED_SI_OP "\tuaword\t"
#endif

#undef TARGET_ASM_FUNCTION_EPILOGUE
#define TARGET_ASM_FUNCTION_EPILOGUE m88k_output_function_epilogue

#undef TARGET_SCHED_ADJUST_COST
#define TARGET_SCHED_ADJUST_COST m88k_adjust_cost

#undef TARGET_HANDLE_OPTION
#define TARGET_HANDLE_OPTION m88k_handle_option

#undef TARGET_STRUCT_VALUE_RTX
#define TARGET_STRUCT_VALUE_RTX m88k_struct_value_rtx

#undef TARGET_PASS_BY_REFERENCE
#define TARGET_PASS_BY_REFERENCE m88k_pass_by_reference

#undef TARGET_RETURN_IN_MEMORY
#define TARGET_RETURN_IN_MEMORY m88k_return_in_memory

#undef TARGET_PROMOTE_PROTOTYPES
#define TARGET_PROMOTE_PROTOTYPES hook_bool_tree_true

#undef TARGET_PROMOTE_FUNCTION_ARGS
#define TARGET_PROMOTE_FUNCTION_ARGS hook_bool_tree_true

#undef TARGET_PROMOTE_FUNCTION_RETURN
#define TARGET_PROMOTE_FUNCTION_RETURN hook_bool_tree_true

#undef TARGET_SETUP_INCOMING_VARARGS
#define TARGET_SETUP_INCOMING_VARARGS m88k_setup_incoming_varargs

#undef TARGET_BUILD_BUILTIN_VA_LIST
#define TARGET_BUILD_BUILTIN_VA_LIST m88k_build_va_list

#undef TARGET_GIMPLIFY_VA_ARG_EXPR
#define TARGET_GIMPLIFY_VA_ARG_EXPR m88k_gimplify_va_arg

#undef TARGET_RTX_COSTS
#define TARGET_RTX_COSTS m88k_rtx_costs

#undef TARGET_ADDRESS_COST
#define TARGET_ADDRESS_COST m88k_address_cost

#undef TARGET_ASM_FILE_START
#define TARGET_ASM_FILE_START m88k_output_file_start
/* from elfos.h
#undef TARGET_ASM_FILE_START_FILE_DIRECTIVE
#define TARGET_ASM_FILE_START_FILE_DIRECTIVE true
*/

struct gcc_target targetm = TARGET_INITIALIZER;

/* Worker function for TARGET_STRUCT_VALUE_RTX.  */

static rtx
m88k_struct_value_rtx (tree fntype ATTRIBUTE_UNUSED,
		       int incoming ATTRIBUTE_UNUSED)
{
  return gen_rtx_REG (Pmode, M88K_STRUCT_VALUE_REGNUM);
}

/* Determine what instructions are needed to manufacture the integer VALUE
   in the given MODE.  */

enum m88k_instruction
classify_integer (enum machine_mode mode, int value)
{
  if (value == 0)
    return m88k_zero;
  else if (SMALL_INTVAL (value))
    return m88k_or;
  else if (SMALL_INTVAL (-value))
    return m88k_subu;
  else if (mode == HImode)
    return m88k_or_lo16;
  else if (mode == QImode)
    return m88k_or_lo8;
  else if (integer_ok_for_set (value))
    return m88k_set;
  else if ((value & 0xffff) == 0)
    return m88k_oru_hi16;
  else
    return m88k_oru_or;
}

/* Return the bit number in a compare word corresponding to CONDITION.  */

int
condition_value (rtx condition)
{
  switch (GET_CODE (condition))
    {
    case UNORDERED:
      return 0;
    case ORDERED:
      return 1;
    case EQ:
      return 2;
    case NE:
      return 3;
    case GT:
      return 4;
    case LE:
      return 5;
    case LT:
      return 6;
    case GE:
      return 7;
    case GTU:
      return 8;
    case LEU:
      return 9;
    case LTU:
      return 10;
    case GEU:
      return 11;
    default:
      gcc_unreachable ();
    }
}

bool
integer_ok_for_set (unsigned int value)
{
  unsigned int mask;

  if (value == 0)
    return false;
  /* All the "one" bits must be contiguous.  If so, MASK + 1 will be
     a power of two or zero.  */
  mask = value | (value - 1);
  return POWER_OF_2_or_0 (mask + 1);
}

const char *
output_load_const_int (enum machine_mode mode, rtx *operands)
{
  static const char *const patterns[] =
    {
      "or %0,%#r0,0",
      "or %0,%#r0,%1",
      "subu %0,%#r0,%n1",
      "or %0,%#r0,%h1",
      "or %0,%#r0,%q1",
      "set %0,%#r0,%s1",
      "or.u %0,%#r0,%X1",
      "or.u %0,%#r0,%X1\n\tor %0,%0,%x1",
    };

  gcc_assert (REG_P (operands[0])
	      && GET_CODE (operands[1]) == CONST_INT);
  return patterns[classify_integer (mode, INTVAL (operands[1]))];
}

/* These next two routines assume that floating point numbers are represented
   in a manner which is consistent between host and target machines.  */

const char *
output_load_const_float (rtx *operands)
{
  /* These can return 0 under some circumstances when cross-compiling.  */
  operands[0] = operand_subword (operands[0], 0, 0, SFmode);
  operands[1] = operand_subword (operands[1], 0, 0, SFmode);

  return output_load_const_int (SImode, operands);
}

const char *
output_load_const_double (rtx *operands)
{
  rtx latehalf[2];

  /* These can return zero on some cross-compilers, but there's nothing
     we can do about it.  */
  latehalf[0] = operand_subword (operands[0], 1, 0, DFmode);
  latehalf[1] = operand_subword (operands[1], 1, 0, DFmode);

  operands[0] = operand_subword (operands[0], 0, 0, DFmode);
  operands[1] = operand_subword (operands[1], 0, 0, DFmode);

  output_asm_insn (output_load_const_int (SImode, operands), operands);

  operands[0] = latehalf[0];
  operands[1] = latehalf[1];

  return output_load_const_int (SImode, operands);
}

const char *
output_load_const_dimode (rtx *operands)
{
  rtx latehalf[2];

  latehalf[0] = operand_subword (operands[0], 1, 0, DImode);
  latehalf[1] = operand_subword (operands[1], 1, 0, DImode);

  operands[0] = operand_subword (operands[0], 0, 0, DImode);
  operands[1] = operand_subword (operands[1], 0, 0, DImode);

  output_asm_insn (output_load_const_int (SImode, operands), operands);

  operands[0] = latehalf[0];
  operands[1] = latehalf[1];

  return output_load_const_int (SImode, operands);
}

/* Emit insns to move operands[1] into operands[0].

   Return 1 if we have written out everything that needs to be done to
   do the move.  Otherwise, return 0 and the caller will emit the move
   normally.

   SCRATCH if nonzero can be used as a scratch register for the move
   operation.  It is provided by a SECONDARY_RELOAD_* macro if needed.  */

int
emit_move_sequence (rtx *operands, enum machine_mode mode, rtx scratch)
{
  rtx operand0 = operands[0];
  rtx operand1 = operands[1];

  if (CONSTANT_P (operand1) && flag_pic
      && pic_address_needs_scratch (operand1))
    operands[1] = operand1 = legitimize_address (1, operand1, NULL_RTX,
						 NULL_RTX);

  /* Handle most common case first: storing into a register.  */
  if (register_operand (operand0, mode))
    {
      if (register_operand (operand1, mode)
	  || (GET_CODE (operand1) == CONST_INT && SMALL_INT (operand1))
	  || GET_CODE (operand1) == HIGH
	  /* Only `general_operands' can come here, so MEM is ok.  */
	  || GET_CODE (operand1) == MEM)
	{
	  /* Run this case quickly.  */
	  emit_insn (gen_rtx_SET (VOIDmode, operand0, operand1));
	  return 1;
	}
    }
  else if (GET_CODE (operand0) == MEM)
    {
      if (register_operand (operand1, mode)
	  || (operand1 == const0_rtx && GET_MODE_SIZE (mode) <= UNITS_PER_WORD))
	{
	  /* Run this case quickly.  */
	  emit_insn (gen_rtx_SET (VOIDmode, operand0, operand1));
	  return 1;
	}
      if (! reload_in_progress && ! reload_completed)
	{
	  operands[0] = validize_mem (operand0);
	  operands[1] = operand1 = force_reg (mode, operand1);
	}
    }

  /* Simplify the source if we need to.  */
  if (GET_CODE (operand1) != HIGH && immediate_operand (operand1, mode))
    {
      if (GET_CODE (operand1) != CONST_INT
	  && GET_CODE (operand1) != CONST_DOUBLE)
	{
	  rtx temp = ((reload_in_progress || reload_completed)
		      ? operand0 : NULL_RTX);
	  operands[1] = legitimize_address (flag_pic
					    && symbolic_address_p (operand1),
					    operand1, temp, scratch);
	  if (mode != SImode)
	    operands[1] = gen_rtx_SUBREG (mode, operands[1], 0);
	}
    }

  /* Now have insn-emit do whatever it normally does.  */
  return 0;
}

/* Return a legitimate reference for ORIG (either an address or a MEM)
   using the register REG.  If PIC and the address is already
   position-independent, use ORIG.  Newly generated position-independent
   addresses go into a reg.  This is REG if nonzero, otherwise we
   allocate register(s) as necessary.  If this is called during reload,
   and we need a second temp register, then we use SCRATCH, which is
   provided via the SECONDARY_INPUT_RELOAD_CLASS mechanism.  */

struct rtx_def *
legitimize_address (int pic, rtx orig, rtx reg, rtx scratch)
{
  rtx addr = (GET_CODE (orig) == MEM ? XEXP (orig, 0) : orig);
  rtx new = orig;
  rtx temp, insn;

  if (pic)
    {
      if (GET_CODE (addr) == SYMBOL_REF || GET_CODE (addr) == LABEL_REF)
	{
	  if (reg == NULL_RTX)
	    {
	      gcc_assert (!reload_in_progress && !reload_completed);
	      reg = gen_reg_rtx (Pmode);
	    }

	  if (flag_pic == 2)
	    {
	      /* If not during reload, allocate another temp reg here for
		 loading in the address, so that these instructions can be
		 optimized properly.  */
	      temp = ((reload_in_progress || reload_completed)
		      ? reg : gen_reg_rtx (Pmode));

	      /* Must put the SYMBOL_REF inside an UNSPEC here so that cse
		 won't get confused into thinking that these two instructions
		 are loading in the true address of the symbol.  If in the
		 future a PIC rtx exists, that should be used instead.  */
	      emit_insn (gen_movsi_high_pic (temp, addr));
	      emit_insn (gen_movsi_lo_sum_pic (temp, temp, addr));
	      addr = temp;
	    }

	  new = gen_rtx_MEM (Pmode,
			     gen_rtx_PLUS (SImode,
					   pic_offset_table_rtx, addr));

	  current_function_uses_pic_offset_table = 1;
	  MEM_READONLY_P (new) = 1;
	  insn = emit_move_insn (reg, new);
	  /* Put a REG_EQUAL note on this insn, so that it can be optimized
	     by loop.  */
	  REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_EQUAL, orig,
						REG_NOTES (insn));
	  new = reg;
	}
      else if (GET_CODE (addr) == CONST)
	{
	  rtx base;

	  if (GET_CODE (XEXP (addr, 0)) == PLUS
	      && XEXP (XEXP (addr, 0), 0) == pic_offset_table_rtx)
	    return orig;

	  if (reg == NULL_RTX)
	    {
	      gcc_assert (!reload_in_progress && !reload_completed);
	      reg = gen_reg_rtx (Pmode);
	    }

	  gcc_assert (GET_CODE (XEXP (addr, 0)) == PLUS);

	  base = legitimize_address (1, XEXP (XEXP (addr, 0), 0), reg,
				     NULL_RTX);
	  addr = legitimize_address (1, XEXP (XEXP (addr, 0), 1),
				     base == reg ? NULL_RTX : reg, NULL_RTX);

	  if (GET_CODE (addr) == CONST_INT)
	    {
	      if (ADD_INT (addr))
		return plus_constant (base, INTVAL (addr));
	      else if (! reload_in_progress && ! reload_completed)
		addr = force_reg (Pmode, addr);
	      /* We can't create any new registers during reload, so use the
		 SCRATCH reg provided by the reload_insi pattern.  */
	      else if (scratch)
		{
		  emit_move_insn (scratch, addr);
		  addr = scratch;
		}
	      else
		/* If we reach here, then the SECONDARY_INPUT_RELOAD_CLASS
		   macro needs to be adjusted so that a scratch reg is provided
		   for this address.  */
		gcc_unreachable ();
	    }
	  new = gen_rtx_PLUS (SImode, base, addr);
	  /* Should we set special REG_NOTEs here?  */
	}
    }
  else
    {
      if (reg == NULL_RTX)
	{
	  gcc_assert (!reload_in_progress && !reload_completed);
	  reg = gen_reg_rtx (Pmode);
	}

      emit_insn (gen_rtx_SET (VOIDmode,
			      reg, gen_rtx_HIGH (SImode, addr)));
      new = gen_rtx_LO_SUM (SImode, reg, addr);
    }

  if (GET_CODE (orig) == MEM)
    {
      new = gen_rtx_MEM (GET_MODE (orig), new);
      MEM_COPY_ATTRIBUTES (new, orig);
    }
  return new;
}

/* Support functions for code to emit a block move.  There are two methods
   used to perform the block move:
   + call memcpy
   + produce an inline sequence of ld/st instructions
 */

static const enum machine_mode mode_from_align[] =
			      {VOIDmode, QImode, HImode, VOIDmode, SImode,
			       VOIDmode, VOIDmode, VOIDmode, DImode};

static void block_move_sequence (rtx, rtx, rtx, rtx, int, int);

/* Emit code to perform a block move.  Choose the best method.

   OPERANDS[0] is the destination.
   OPERANDS[1] is the source.
   OPERANDS[2] is the size.
   OPERANDS[3] is the alignment safe to use.  */

void
expand_block_move (rtx dest_mem, rtx src_mem, rtx *operands)
{
  int align = INTVAL (operands[3]);
  int constp = (GET_CODE (operands[2]) == CONST_INT);
  int bytes = (constp ? INTVAL (operands[2]) : 0);

  if (constp && bytes <= 0)
    return;

  /* Determine machine mode to do move with.  */
  if (align > 4 && !TARGET_88110)
    align = 4;
  else
    gcc_assert (align > 0 && align != 3); /* block move invalid alignment.  */

  if (constp && bytes <= 3 * align)
    block_move_sequence (operands[0], dest_mem, operands[1], src_mem,
			 bytes, align);

  else
    {
      emit_library_call (gen_rtx_SYMBOL_REF (Pmode, "memcpy"), 0,
			 VOIDmode, 3,
			 operands[0], Pmode,
			 operands[1], Pmode,
			 convert_to_mode (TYPE_MODE (sizetype), operands[2],
					  TYPE_UNSIGNED (sizetype)),
			 TYPE_MODE (sizetype));
    }
}

/* Emit code to perform a block move with an offset sequence of ld/st
   instructions (..., ld 0, st 1, ld 1, st 0, ...).  SIZE and ALIGN are
   known constants.  DEST and SRC are registers.  */

static void
block_move_sequence (rtx dest, rtx dest_mem, rtx src, rtx src_mem, int size,
		     int align)
{
  rtx temp[2];
  enum machine_mode mode[2];
  int amount[2];
  int active[2];
  int phase = 0;
  int next;
  int offset_ld = 0;
  int offset_st = 0;

  active[0] = active[1] = FALSE;

  /* Establish parameters for the first load and for the second load if
     it is known to be the same mode as the first.  */
  amount[0] = amount[1] = align;
  mode[0] = mode_from_align[align];
  temp[0] = gen_reg_rtx (mode[0]);
  if (size >= 2 * align)
    {
      mode[1] = mode[0];
      temp[1] = gen_reg_rtx (mode[1]);
    }

  do
    {
      next = phase;
      phase = !phase;

      if (size > 0)
	{
	  /* Change modes as the sequence tails off.  */
	  if (size < amount[next])
	    {
	      amount[next] = (size >= 4 ? 4 : (size >= 2 ? 2 : 1));
	      mode[next] = mode_from_align[amount[next]];
	      temp[next] = gen_reg_rtx (mode[next]);
	    }
	  size -= amount[next];
	  emit_move_insn (temp[next],
			  adjust_address (src_mem, mode[next], offset_ld));
	  offset_ld += amount[next];
	  active[next] = TRUE;
	}

      if (active[phase])
	{
	  active[phase] = FALSE;
	  emit_move_insn (adjust_address (dest_mem, mode[phase], offset_st),
			  temp[phase]);
	  offset_st += amount[phase];
	}
    }
  while (active[next]);
}

/* Emit the code to do an AND operation.  */

const char *
output_and (rtx operands[])
{
  unsigned int value;

  if (REG_P (operands[2]))
    return "and %0,%1,%2";

  value = INTVAL (operands[2]);
  if (SMALL_INTVAL (value))
    return "mask %0,%1,%2";
  else if ((value & 0xffff0000) == 0xffff0000)
    return "and %0,%1,%x2";
  else if ((value & 0xffff) == 0xffff)
    return "and.u %0,%1,%X2";
  else if ((value & 0xffff) == 0)
    return "mask.u %0,%1,%X2";
  else if (integer_ok_for_set (~value))
    return "clr %0,%1,%S2";
  else
    return "and.u %0,%1,%X2\n\tand %0,%0,%x2";
}

/* Emit the code to do an inclusive OR operation.  */

const char *
output_ior (rtx operands[])
{
  unsigned int value;

  if (REG_P (operands[2]))
    return "or %0,%1,%2";

  value = INTVAL (operands[2]);
  if (SMALL_INTVAL (value))
    return "or %0,%1,%2";
  else if ((value & 0xffff) == 0)
    return "or.u %0,%1,%X2";
  else if (integer_ok_for_set (value))
    return "set %0,%1,%s2";
  else
    return "or.u %0,%1,%X2\n\tor %0,%0,%x2";
}

/* Emit the instructions for doing an XOR.  */

const char *
output_xor (rtx operands[])
{
  unsigned int value;

  if (REG_P (operands[2]))
    return "xor %0,%1,%2";

  value = INTVAL (operands[2]);
  if (SMALL_INTVAL (value))
    return "xor %0,%1,%2";
  else if ((value & 0xffff) == 0)
    return "xor.u %0,%1,%X2";
  else
    return "xor.u %0,%1,%X2\n\txor %0,%0,%x2";
}

/* Output a call.  Normally this is just bsr or jsr, but this also deals with
   accomplishing a branch after the call by incrementing r1.

   It would seem the same idea could be used to tail call, but in this case,
   the epilogue will be non-null.  */

const char *
output_call (rtx operands[], rtx addr)
{
  operands[0] = addr;
  if (final_sequence)
    {
      rtx jump;
      rtx seq_insn;

      /* This can be generalized, but there is currently no need.  */
      gcc_assert (XVECLEN (final_sequence, 0) == 2);

      /* The address of interior insns is not computed, so use the sequence.  */
      seq_insn = NEXT_INSN (PREV_INSN (XVECEXP (final_sequence, 0, 0)));
      jump = XVECEXP (final_sequence, 0, 1);
      if (GET_CODE (jump) == JUMP_INSN)
	{
	  const char *last;
	  rtx dest = XEXP (SET_SRC (PATTERN (jump)), 0);
	  int delta = 4 * (INSN_ADDRESSES (INSN_UID (dest))
			   - INSN_ADDRESSES (INSN_UID (seq_insn))
			   - 2);

	  /* Delete the jump.  */
	  PUT_CODE (jump, NOTE);
	  NOTE_LINE_NUMBER (jump) = NOTE_INSN_DELETED;
	  NOTE_SOURCE_FILE (jump) = 0;

	  /* We only do this optimization if -O2, modifying the value of
	     r1 in the delay slot confuses debuggers and profilers on some
	     systems.

	     If we loose, we must use the non-delay form.  This is unlikely
	     to ever happen.  If it becomes a problem, claim that a call
	     has two delay slots and only the second can be filled with
	     a jump.  

	     The 88110 can lose when a jsr.n r1 is issued and a page fault
	     occurs accessing the delay slot.  So don't use jsr.n form when
	     jumping thru r1.
	   */
	  if (optimize < 2
	      || ! ADD_INTVAL (delta)
	      || (REG_P (addr) && REGNO (addr) == 1))
	    {
	      operands[1] = dest;
	      return (REG_P (addr)
		      ? "jsr %0\n\tbr %l1"
		      : (flag_pic
			 ? "bsr %0#plt\n\tbr %l1"
			 : "bsr %0\n\tbr %l1"));
	    }

	  /* Output the short branch form.  */
	  output_asm_insn ((REG_P (addr)
			    ? "jsr.n %0"
			    : (flag_pic ? "bsr.n %0#plt" : "bsr.n %0")),
			   operands);

	  last = (delta < 0
		  ? "subu %#r1,%#r1,.-%l0+4"
		  : "addu %#r1,%#r1,%l0-.-4");
	  operands[0] = dest;

	  return last;
	}
    }
  return (REG_P (addr)
	  ? "jsr%. %0"
	  : (flag_pic ? "bsr%. %0#plt" : "bsr%. %0"));
}

/* Return truth value of the statement that this conditional branch is likely
   to fall through.  CONDITION, is the condition that JUMP_INSN is testing.  */

bool
mostly_false_jump (rtx jump_insn, rtx condition)
{
  rtx target_label = JUMP_LABEL (jump_insn);
  rtx insnt, insnj;

  /* Much of this isn't computed unless we're optimizing.  */
  if (optimize == 0)
    return false;

  /* Determine if one path or the other leads to a return.  */
  for (insnt = NEXT_INSN (target_label);
       insnt;
       insnt = NEXT_INSN (insnt))
    {
      if (GET_CODE (insnt) == JUMP_INSN)
	break;
      else if (GET_CODE (insnt) == INSN
	       && GET_CODE (PATTERN (insnt)) == SEQUENCE
	       && GET_CODE (XVECEXP (PATTERN (insnt), 0, 0)) == JUMP_INSN)
	{
	  insnt = XVECEXP (PATTERN (insnt), 0, 0);
	  break;
	}
    }
  if (insnt
      && (GET_CODE (PATTERN (insnt)) == RETURN
	  || (GET_CODE (PATTERN (insnt)) == SET
	      && GET_CODE (SET_SRC (PATTERN (insnt))) == REG
	      && REGNO (SET_SRC (PATTERN (insnt))) == 1)))
    insnt = NULL_RTX;

  for (insnj = NEXT_INSN (jump_insn);
       insnj;
       insnj = NEXT_INSN (insnj))
    {
      if (GET_CODE (insnj) == JUMP_INSN)
	break;
      else if (GET_CODE (insnj) == INSN
	       && GET_CODE (PATTERN (insnj)) == SEQUENCE
	       && GET_CODE (XVECEXP (PATTERN (insnj), 0, 0)) == JUMP_INSN)
	{
	  insnj = XVECEXP (PATTERN (insnj), 0, 0);
	  break;
	}
    }
  if (insnj
      && (GET_CODE (PATTERN (insnj)) == RETURN
	  || (GET_CODE (PATTERN (insnj)) == SET
	      && GET_CODE (SET_SRC (PATTERN (insnj))) == REG
	      && REGNO (SET_SRC (PATTERN (insnj))) == 1)))
    insnj = NULL_RTX;

  /* Predict to not return.  */
  if ((insnt == NULL_RTX) != (insnj == NULL_RTX))
    return (insnt == NULL_RTX);

  /* Predict loops to loop.  */
  for (insnt = PREV_INSN (target_label);
       insnt && GET_CODE (insnt) == NOTE;
       insnt = PREV_INSN (insnt))
    if (NOTE_LINE_NUMBER (insnt) == NOTE_INSN_LOOP_END)
      return true;
    else if (NOTE_LINE_NUMBER (insnt) == NOTE_INSN_LOOP_BEG)
      return false;

  /* Predict backward branches usually take.  */
  if (final_sequence)
    insnj = NEXT_INSN (PREV_INSN (XVECEXP (final_sequence, 0, 0)));
  else
    insnj = jump_insn;
  if (INSN_ADDRESSES (INSN_UID (insnj))
      > INSN_ADDRESSES (INSN_UID (target_label)))
    return false;

  /* EQ tests are usually false and NE tests are usually true.  Also,
     most quantities are positive, so we can make the appropriate guesses
     about signed comparisons against zero.  Consider unsigned comparisons
     to be a range check and assume quantities to be in range.  */
  switch (GET_CODE (condition))
    {
    case CONST_INT:
      /* Unconditional branch.  */
      return false;
    case EQ:
      return true;
    case NE:
      return false;
    case LE:
    case LT:
    case GEU:
    case GTU: /* Must get casesi right at least.  */
      if (XEXP (condition, 1) == const0_rtx)
        return true;
      break;
    case GE:
    case GT:
    case LEU:
    case LTU:
      if (XEXP (condition, 1) == const0_rtx)
	return false;
      break;
    default:
      break;
    }

  return false;
}

/* Return true if the operand is a power of two and is a floating
   point type (to optimize division by power of two into multiplication).  */

bool
real_power_of_2_operand (rtx op)
{
  REAL_VALUE_TYPE d;
  union {
    long l[2];
    struct {				/* IEEE double precision format */
      unsigned sign	 :  1;
      unsigned exponent  : 11;
      unsigned mantissa1 : 20;
      unsigned mantissa2;
    } s;
    struct {				/* IEEE double format to quick check */
      unsigned sign	 :  1;		/* if it fits in a float */
      unsigned exponent1 :  4;
      unsigned exponent2 :  7;
      unsigned mantissa1 : 20;
      unsigned mantissa2;
    } s2;
  } u;

  if (GET_MODE (op) != DFmode && GET_MODE (op) != SFmode)
    return false;

  if (GET_CODE (op) != CONST_DOUBLE)
    return false;

  REAL_VALUE_FROM_CONST_DOUBLE (d, op);
  REAL_VALUE_TO_TARGET_DOUBLE (d, u.l);

  if (u.s.mantissa1 != 0 || u.s.mantissa2 != 0	/* not a power of two */
      || u.s.exponent == 0			/* constant 0.0 */
      || u.s.exponent == 0x7ff			/* NaN */
      || (u.s2.exponent1 != 0x8 && u.s2.exponent1 != 0x7))
    return false;				/* const won't fit in float */

  return true;
}

/* Make OP legitimate for mode MODE.  Currently this only deals with DFmode
   operands, putting them in registers and making CONST_DOUBLE values
   SFmode where possible.  */

struct rtx_def *
legitimize_operand (rtx op, enum machine_mode mode)
{
  rtx temp;
  REAL_VALUE_TYPE d;
  union {
    long l[2];
    struct {				/* IEEE double precision format */
      unsigned sign	 :  1;
      unsigned exponent  : 11;
      unsigned mantissa1 : 20;
      unsigned mantissa2;
    } s;
    struct {				/* IEEE double format to quick check */
      unsigned sign	 :  1;		/* if it fits in a float */
      unsigned exponent1 :  4;
      unsigned exponent2 :  7;
      unsigned mantissa1 : 20;
      unsigned mantissa2;
    } s2;
  } u;

  if (GET_CODE (op) == REG || mode != DFmode)
    return op;

  if (GET_CODE (op) == CONST_DOUBLE)
    {
      REAL_VALUE_FROM_CONST_DOUBLE (d, op);
      REAL_VALUE_TO_TARGET_DOUBLE (d, u.l);
      if (u.s.exponent != 0x7ff /* NaN */
	  && u.s.mantissa2 == 0 /* Mantissa fits */
	  && (u.s2.exponent1 == 0x8 || u.s2.exponent1 == 0x7) /* Exponent fits */
	  && (temp = simplify_unary_operation (FLOAT_TRUNCATE, SFmode,
					       op, mode)) != 0)
	return gen_rtx_FLOAT_EXTEND (mode, force_reg (SFmode, temp));
    }
  else if (register_operand (op, mode))
    return op;

  return force_reg (mode, op);
}

/* Returns true if OP is either a symbol reference or a sum of a symbol
   reference and a constant.  */

bool
symbolic_address_p (rtx op)
{
  switch (GET_CODE (op))
    {
    case SYMBOL_REF:
    case LABEL_REF:
      return true;

    case CONST:
      op = XEXP (op, 0);
      return ((GET_CODE (XEXP (op, 0)) == SYMBOL_REF
	       || GET_CODE (XEXP (op, 0)) == LABEL_REF)
	      && GET_CODE (XEXP (op, 1)) == CONST_INT);

    default:
      return false;
    }
}

/* Nonzero if this is a bitmask filling the bottom bits, for optimizing and +
   shift left combinations into a single mak instruction.  */

bool
mak_mask_p (int value)
{
  return (value && POWER_OF_2_or_0 (value + 1));
}

/* Output to FILE the start of the assembler file.  */

static void
m88k_output_file_start (void)
{
  if (TARGET_88110)
    fprintf (asm_out_file, "%s\n", REQUIRES_88110_ASM_OP);

  default_file_start ();
}

/* Generate the assembly code for function entry.

   The prologue is responsible for setting up the stack frame,
   initializing the frame pointer register, saving registers that must be
   saved, and allocating SIZE additional bytes of storage for the
   local variables.  SIZE is an integer.  FILE is a stdio
   stream to which the assembler code should be output.

   The label for the beginning of the function need not be output by this
   macro.  That has already been done when the macro is run.

   To determine which registers to save, the macro can refer to the array
   `regs_ever_live': element R is nonzero if hard register
   R is used anywhere within the function.  This implies the
   function prologue should save register R, but not if it is one
   of the call-used registers.

   On machines where functions may or may not have frame-pointers, the
   function entry code must vary accordingly; it must set up the frame
   pointer if one is wanted, and not otherwise.  To determine whether a
   frame pointer is in wanted, the macro can refer to the variable
   `frame_pointer_needed'.  The variable's value will be 1 at run
   time in a function that needs a frame pointer.

   On machines where an argument may be passed partly in registers and
   partly in memory, this macro must examine the variable
   `current_function_pretend_args_size', and allocate that many bytes
   of uninitialized space on the stack just underneath the first argument
   arriving on the stack.  (This may not be at the very end of the stack,
   if the calling sequence has pushed anything else since pushing the stack
   arguments.  But usually, on such machines, nothing else has been pushed
   yet, because the function prologue itself does all the pushing.)

   If `ACCUMULATE_OUTGOING_ARGS' is defined, the variable
   `current_function_outgoing_args_size' contains the size in bytes
   required for the outgoing arguments.  This macro must add that
   amount of uninitialized space to very bottom of the stack.

   The stack frame we use looks like this:

 caller                                                  callee
        |==============================================|
        |                caller's frame                |
        |==============================================|
        |     [caller's outgoing memory arguments]     |
  sp -> |==============================================| <- ap
        |            [local variable space]            |
        |----------------------------------------------|
        |            [return address (r1)]             |
        |----------------------------------------------|
        |        [previous frame pointer (r30)]        |
        |==============================================| <- fp
        |       [preserved registers (r25..r14)]       |
        |----------------------------------------------|
        |       [preserved registers (x29..x22)]       |
        |==============================================|
        |    [dynamically allocated space (alloca)]    |
        |==============================================|
        |     [callee's outgoing memory arguments]     |
        |==============================================| <- sp

  Notes:

  r1 and r30 must be saved if debugging.

  fp (if present) is located two words down from the local
  variable space.
  */

static rtx emit_add (rtx, rtx, int);
static void preserve_registers (int, int);
static void emit_ldst (int, int, enum machine_mode, int);

static int  nregs;
static int  nxregs;
static char save_regs[FIRST_PSEUDO_REGISTER];
static int  frame_laid_out;
static int  frame_size;

#define STACK_UNIT_BOUNDARY (STACK_BOUNDARY / BITS_PER_UNIT)
#define ROUND_CALL_BLOCK_SIZE(BYTES) \
  (((BYTES) + (STACK_UNIT_BOUNDARY - 1)) & ~(STACK_UNIT_BOUNDARY - 1))

/* Establish the position of the FP relative to the SP.  This is done
   either during output_function_prologue() or by
   INITIAL_ELIMINATION_OFFSET.  */

void
m88k_layout_frame (void)
{
  int regno, sp_size;

  if (frame_laid_out && reload_completed)
    return;

  frame_laid_out = 1;

  memset ((char *) &save_regs[0], 0, sizeof (save_regs));
  sp_size = nregs = nxregs = 0;
  frame_size = get_frame_size ();

  /* Profiling requires a stack frame.  */
  if (current_function_profile)
    frame_pointer_needed = 1;

  /* If we are producing debug information, store r1 and r30 where the
     debugger wants to find them (r30 at r30+0, r1 at r30+4).  Space has
     already been reserved for r1/r30 in STARTING_FRAME_OFFSET.  */
  if (write_symbols != NO_DEBUG)
    save_regs[1] = 1;

  /* If we are producing PIC, save the addressing base register and r1.  */
  if (flag_pic && current_function_uses_pic_offset_table)
    {
      save_regs[PIC_OFFSET_TABLE_REGNUM] = 1;
      nregs++;
    }

  /* If a frame is requested, save the previous FP, and the return
     address (r1), so that a traceback can be done without using tdesc
     information.  Otherwise, simply save the FP if it is used as
     a preserve register.  */
  if (frame_pointer_needed)
    save_regs[FRAME_POINTER_REGNUM] = save_regs[1] = 1;
  else
    {
      if (regs_ever_live[FRAME_POINTER_REGNUM])
	save_regs[FRAME_POINTER_REGNUM] = 1;
      /* If there is a call, r1 needs to be saved as well.  */
      if (regs_ever_live[1])
	save_regs[1] = 1;
    }

  /* Figure out which extended register(s) needs to be saved.  */
  for (regno = FIRST_EXTENDED_REGISTER + 1; regno < FIRST_PSEUDO_REGISTER;
       regno++)
    if (regs_ever_live[regno] && ! call_used_regs[regno])
      {
	save_regs[regno] = 1;
	nxregs++;
      }

  /* Figure out which normal register(s) needs to be saved.  */
  for (regno = 2; regno < FRAME_POINTER_REGNUM; regno++)
    if (regs_ever_live[regno] && ! call_used_regs[regno])
      {
	save_regs[regno] = 1;
	nregs++;
      }

  /* Achieve greatest use of double memory ops.  Either we end up saving
     r30 or we use that slot to align the registers we do save.  */
  if (nregs >= 2 && save_regs[1] && !save_regs[FRAME_POINTER_REGNUM])
    sp_size += 4;

  nregs += save_regs[1] + save_regs[FRAME_POINTER_REGNUM];
  /* if we need to align extended registers, add a word */
  if (nxregs > 0 && (nregs & 1) != 0)
    sp_size +=4;
  sp_size += 4 * nregs;
  sp_size += 8 * nxregs;
  sp_size += current_function_outgoing_args_size;

  /* The first two saved registers are placed above the new frame pointer
     if any.  In the only case this matters, they are r1 and r30. */
  if (frame_pointer_needed || sp_size)
    m88k_fp_offset = ROUND_CALL_BLOCK_SIZE (sp_size - STARTING_FRAME_OFFSET);
  else
    m88k_fp_offset = -STARTING_FRAME_OFFSET;
  m88k_stack_size = m88k_fp_offset + STARTING_FRAME_OFFSET;

  /* First, combine m88k_stack_size and size.  If m88k_stack_size is
     nonzero, align the frame size to 8 mod 16; otherwise align the
     frame size to 0 mod 16.  (If stacks are 8 byte aligned, this ends
     up as a NOP.  */
  {
    int need
      = ((m88k_stack_size ? STACK_UNIT_BOUNDARY - STARTING_FRAME_OFFSET : 0)
	 - (frame_size % STACK_UNIT_BOUNDARY));
    if (need < 0)
      need += STACK_UNIT_BOUNDARY;
    m88k_stack_size
      = ROUND_CALL_BLOCK_SIZE (m88k_stack_size + frame_size + need
			       + current_function_pretend_args_size);
  }
}

/* Return true if this function is known to have a null prologue.  */

bool
null_prologue (void)
{
  if (! reload_completed)
    return false;
  m88k_layout_frame ();
  return (! frame_pointer_needed
	  && nregs == 0
	  && nxregs == 0
	  && m88k_stack_size == 0);
}

static void
m88k_maybe_dead (rtx insn)
{
  REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_MAYBE_DEAD,
					const0_rtx,
					REG_NOTES (insn));
}

void
m88k_expand_prologue (void)
{
  rtx insn;

  m88k_layout_frame ();

  if (warn_stack_larger_than && m88k_stack_size > stack_larger_than_size)
    warning (0, "stack usage is %d bytes", m88k_stack_size);

  if (m88k_stack_size)
    {
      insn = emit_add (stack_pointer_rtx, stack_pointer_rtx, -m88k_stack_size);
      RTX_FRAME_RELATED_P (insn) = 1;

      /* If the stack pointer adjustment has required a temporary register,
	 tell the DWARF code how to understand this sequence.  */
      if (! ADD_INTVAL (m88k_stack_size))
	REG_NOTES (insn)
	  = gen_rtx_EXPR_LIST (REG_FRAME_RELATED_EXPR,
			       gen_rtx_SET (VOIDmode, stack_pointer_rtx,
				     gen_rtx_PLUS (Pmode, stack_pointer_rtx,
						   GEN_INT (-m88k_stack_size))),
			       REG_NOTES(insn));
    }

  if (nregs || nxregs)
    preserve_registers (m88k_fp_offset + 4, 1);

  if (frame_pointer_needed)
    {
      /* Be sure to emit this instruction after all register saves, DWARF
	 information depends on this.  */
      emit_insn (gen_blockage ());
      insn = emit_add (frame_pointer_rtx, stack_pointer_rtx, m88k_fp_offset);
      RTX_FRAME_RELATED_P (insn) = 1;
    }

  if (flag_pic && save_regs[PIC_OFFSET_TABLE_REGNUM])
    {
      rtx return_reg = gen_rtx_REG (SImode, 1);
      rtx label = gen_label_rtx ();

      m88k_maybe_dead (emit_insn (gen_locate1 (pic_offset_table_rtx, label)));
      m88k_maybe_dead (emit_insn (gen_locate2 (pic_offset_table_rtx, label)));
      m88k_maybe_dead (emit_insn (gen_addsi3 (pic_offset_table_rtx,
					      pic_offset_table_rtx,
					      return_reg)));
    }
  if (current_function_profile)
    emit_insn (gen_blockage ());
}

/* This function generates the assembly code for function exit,
   on machines that need it.

   The function epilogue should not depend on the current stack pointer!
   It should use the frame pointer only, if there is a frame pointer.
   This is mandatory because of alloca; we also take advantage of it to
   omit stack adjustments before returning.  */

static void
m88k_output_function_epilogue (FILE *stream,
			       HOST_WIDE_INT size ATTRIBUTE_UNUSED)
{ 
  frame_laid_out = 0;
}

void
m88k_expand_epilogue (void)
{
  if (frame_pointer_needed)
    {
      emit_insn (gen_blockage ());
      emit_add (stack_pointer_rtx, frame_pointer_rtx, -m88k_fp_offset);
    }

  if (nregs || nxregs)
    preserve_registers (m88k_fp_offset + 4, 0);

  emit_insn (gen_blockage ());

  if (m88k_stack_size)
    {
      emit_add (stack_pointer_rtx, stack_pointer_rtx, m88k_stack_size);
    }

  emit_insn (gen_indirect_jump (INCOMING_RETURN_ADDR_RTX));

  emit_insn (gen_blockage ());
}

/* Emit insns to set DSTREG to SRCREG + AMOUNT during the prologue or
   epilogue.  */

static rtx
emit_add (rtx dstreg, rtx srcreg, int amount)
{
  rtx incr = GEN_INT (abs (amount));

  if (! ADD_INTVAL (amount))
    {
      rtx temp = gen_rtx_REG (SImode, TEMP_REGNUM);
      emit_move_insn (temp, incr);
      incr = temp;
    }
  return emit_insn ((amount < 0 ? gen_subsi3 : gen_addsi3) (dstreg, srcreg,
							    incr));
}

/* Save/restore the preserve registers.  base is the highest offset from
   r31 at which a register is stored.  store_p is true if stores are to
   be done; otherwise loads.  */

static void
preserve_registers (int base, int store_p)
{
  int regno, offset;
  struct mem_op {
    int regno;
    int nregs;
    int offset;
  } mem_op[FIRST_PSEUDO_REGISTER];
  struct mem_op *mo_ptr = mem_op;

  /* The 88open OCS mandates that preserved registers be stored in
     increasing order.  For compatibility with current practice,
     the order is r1, r30, then the preserve registers.  */

  offset = base;
  if (save_regs[1])
    {
      /* An extra word is given in this case to make best use of double
	 memory ops.  */
      if (nregs > 2 && !save_regs[FRAME_POINTER_REGNUM])
	offset -= 4;
      /* Do not reload r1 in the epilogue unless really necessary */
      if (store_p || regs_ever_live[1]
	  || (flag_pic && save_regs[PIC_OFFSET_TABLE_REGNUM]))
	emit_ldst (store_p, 1, SImode, offset);
      offset -= 4;
      base = offset;
    }

  /* Walk the registers to save recording all single memory operations.  */
  for (regno = FRAME_POINTER_REGNUM; regno > 1; regno--)
    if (save_regs[regno])
      {
	if ((offset & 7) != 4 || (regno & 1) != 1 || !save_regs[regno-1])
	  {
	    mo_ptr->nregs = 1;
	    mo_ptr->regno = regno;
	    mo_ptr->offset = offset;
	    mo_ptr++;
	    offset -= 4;
	  }
        else
	  {
	    regno--;
	    offset -= 2*4;
	  }
      }

  /* Walk the registers to save recording all double memory operations.
     This avoids a delay in the epilogue (ld.d/ld).  */
  offset = base;
  for (regno = FRAME_POINTER_REGNUM; regno > 1; regno--)
    if (save_regs[regno])
      {
	if ((offset & 7) != 4 || (regno & 1) != 1 || !save_regs[regno-1])
	  {
	    offset -= 4;
	  }
        else
	  {
	    mo_ptr->nregs = 2;
	    mo_ptr->regno = regno-1;
	    mo_ptr->offset = offset-4;
	    mo_ptr++;
	    regno--;
	    offset -= 2*4;
	  }
      }

  /* Walk the extended registers to record all memory operations.  */
  /*  Be sure the offset is double word aligned.  */
  offset = (offset - 1) & ~7;
  for (regno = FIRST_PSEUDO_REGISTER - 1; regno > FIRST_EXTENDED_REGISTER;
       regno--)
    if (save_regs[regno])
      {
	mo_ptr->nregs = 2;
	mo_ptr->regno = regno;
	mo_ptr->offset = offset;
	mo_ptr++;
	offset -= 2*4;
      }

  mo_ptr->regno = 0;

  /* Output the memory operations.  */
  for (mo_ptr = mem_op; mo_ptr->regno; mo_ptr++)
    {
      if (mo_ptr->nregs)
	emit_ldst (store_p, mo_ptr->regno,
		   (mo_ptr->nregs > 1 ? DImode : SImode),
		   mo_ptr->offset);
    }
}

static void
emit_ldst (int store_p, int regno, enum machine_mode mode, int offset)
{
  rtx reg = gen_rtx_REG (mode, regno);
  rtx mem;
  rtx insn;

  if (SMALL_INTVAL (offset))
    {
      mem = gen_rtx_MEM (mode, plus_constant (stack_pointer_rtx, offset));
    }
  else
    {
      /* offset is too large for immediate index must use register */

      rtx disp = GEN_INT (offset);
      rtx temp = gen_rtx_REG (SImode, TEMP_REGNUM);
      rtx regi = gen_rtx_PLUS (SImode, stack_pointer_rtx, temp);

      emit_move_insn (temp, disp);
      mem = gen_rtx_MEM (mode, regi);
    }

  if (store_p)
    {
      insn = emit_move_insn (mem, reg);
      RTX_FRAME_RELATED_P (insn) = 1;
    }
  else
    emit_move_insn (reg, mem);
}

/* Convert the address expression REG to a CFA offset.  */

int
m88k_debugger_offset (rtx reg, int offset)
{
  if (GET_CODE (reg) == PLUS)
    {
      offset = INTVAL (XEXP (reg, 1));
      reg = XEXP (reg, 0);
    }

  /* Put the offset in terms of the CFA (arg pointer).  */
  if (reg == frame_pointer_rtx)
    offset += m88k_fp_offset - m88k_stack_size;
  else if (reg == stack_pointer_rtx)
    offset -= m88k_stack_size;
  else if (reg != arg_pointer_rtx)
    return 0;

  return offset;
}

/* Output assembler code to FILE to increment profiler label # LABELNO
   for profiling a function entry.  NAME is the mcount function name
   (varies).  */

void
output_function_profiler (FILE *file, int labelno, const char *name)
{
  char label[256];

  /* Remember to update FUNCTION_PROFILER_LENGTH.  */

  asm_fprintf (file, "\tsubu\t %R%s,%R%s,32\n", reg_names[31], reg_names[31]);
  asm_fprintf (file, "\tst.d\t %R%s,%R%s,0\n", reg_names[2], reg_names[31]);
  asm_fprintf (file, "\tst.d\t %R%s,%R%s,8\n", reg_names[4], reg_names[31]);
  asm_fprintf (file, "\tst.d\t %R%s,%R%s,16\n", reg_names[6], reg_names[31]);
  asm_fprintf (file, "\tst.d\t %R%s,%R%s,24\n", reg_names[8], reg_names[31]);

  ASM_GENERATE_INTERNAL_LABEL (label, "LP", labelno);
  if (flag_pic == 2)
    {
      asm_fprintf (file, "\tor.u\t %R%s,%R%s,%Rhi16(%s#got_rel)\n",
		   reg_names[2], reg_names[0], &label[1]);
      asm_fprintf (file, "\tor\t %R%s,%R%s,%Rlo16(%s#got_rel)\n",
		   reg_names[2], reg_names[2], &label[1]);
      asm_fprintf (file, "\tbsr.n\t %s#plt\n", name);
      asm_fprintf (file, "\t ld\t %R%s,%R%s,%R%s\n", reg_names[2],
		   reg_names[PIC_OFFSET_TABLE_REGNUM], reg_names[2]);
    }
  else if (flag_pic)
    {
      asm_fprintf (file, "\tbsr.n\t %s#plt\n", name);
      asm_fprintf (file, "\t ld\t %R%s,%R%s,%s#got_rel\n", reg_names[2],
		   reg_names[PIC_OFFSET_TABLE_REGNUM], &label[1]);
    }
  else
    {
      asm_fprintf (file, "\tor.u\t %R%s,%R%s,%Rhi16(%s)\n",
		   reg_names[2], reg_names[0], &label[1]);
      asm_fprintf (file, "\tbsr.n\t %s\n", name);
      asm_fprintf (file, "\t or\t %R%s,%R%s,%Rlo16(%s)\n",
		   reg_names[2], reg_names[2], &label[1]);
    }

  asm_fprintf (file, "\tld.d\t %R%s,%R%s,0\n", reg_names[2], reg_names[31]);
  asm_fprintf (file, "\tld.d\t %R%s,%R%s,8\n", reg_names[4], reg_names[31]);
  asm_fprintf (file, "\tld.d\t %R%s,%R%s,16\n", reg_names[6], reg_names[31]);
  asm_fprintf (file, "\tld.d\t %R%s,%R%s,24\n", reg_names[8], reg_names[31]);
  asm_fprintf (file, "\taddu\t %R%s,%R%s,32\n", reg_names[31], reg_names[31]);
}

/* Determine whether a function argument is passed in a register, and
   which register.

   The arguments are CUM, which summarizes all the previous
   arguments; MODE, the machine mode of the argument; TYPE,
   the data type of the argument as a tree node or 0 if that is not known
   (which happens for C support library functions); and NAMED,
   which is 1 for an ordinary argument and 0 for nameless arguments that
   correspond to `...' in the called function's prototype.

   The value of the expression should either be a `reg' RTX for the
   hard register in which to pass the argument, or zero to pass the
   argument on the stack.

   On the m88000 the first eight words of args are normally in registers
   and the rest are pushed.  Double precision floating point must be
   double word aligned (and if in a register, starting on an even
   register). Structures and unions which are not 4 byte, and word
   aligned are passed in memory rather than registers, even if they
   would fit completely in the registers under OCS rules.

   Note that FUNCTION_ARG and FUNCTION_INCOMING_ARG were different.
   For structures that are passed in memory, but could have been
   passed in registers, we first load the structure into the
   register, and then when the last argument is passed, we store
   the registers into the stack locations.  This fixes some bugs
   where GCC did not expect to have register arguments, followed
   by stack arguments, followed by register arguments.  */

struct rtx_def *
m88k_function_arg (CUMULATIVE_ARGS args_so_far, enum machine_mode mode,
		   tree type, int named ATTRIBUTE_UNUSED)
{
  int bytes, words;

  /* undo putting struct in register */
  if (type != NULL_TREE && AGGREGATE_TYPE_P (type))
    mode = BLKmode;

  /* m88k_function_arg argument `type' is NULL for BLKmode. */
  gcc_assert (type != NULL_TREE || mode != BLKmode);

  bytes = (mode != BLKmode) ? GET_MODE_SIZE (mode) : int_size_in_bytes (type);

  /* Variable-sized types get passed by reference, which can be passed
     in registers.  */
  if (bytes < 0)
    {
      if (args_so_far > 8 - (POINTER_SIZE / BITS_PER_WORD))
	return NULL_RTX;

      return gen_rtx_REG (Pmode, 2 + args_so_far);
    }

  words = (bytes + UNITS_PER_WORD - 1) / UNITS_PER_WORD;

  if ((args_so_far & 1) != 0
      && (mode == DImode || mode == DFmode
	  || (type != NULL_TREE && TYPE_ALIGN (type) > BITS_PER_WORD)))
    args_so_far++;

  if (args_so_far + words > 8)
    return NULL_RTX;			/* args have exhausted registers */

  else if (mode == BLKmode
	   && (TYPE_ALIGN (type) != BITS_PER_WORD || bytes != UNITS_PER_WORD))
    return NULL_RTX;

  return gen_rtx_REG (((mode == BLKmode) ? TYPE_MODE (type) : mode),
		      2 + args_so_far);
}

/* Update the summarizer variable CUM to advance past an argument in
   the argument list.  The values MODE, TYPE and NAMED describe that
   argument.  Once this is done, the variable CUM is suitable for
   analyzing the *following* argument with `FUNCTION_ARG', etc.  (TYPE
   is null for libcalls where that information may not be available.)  */
void
m88k_function_arg_advance (CUMULATIVE_ARGS *args_so_far, enum machine_mode mode,
			   tree type, int named ATTRIBUTE_UNUSED)
{
  int bytes, words;
  int asf;

  if (type != NULL_TREE && AGGREGATE_TYPE_P (type))
    mode = BLKmode;

  bytes = (mode != BLKmode) ? GET_MODE_SIZE (mode) : int_size_in_bytes (type);
  asf = *args_so_far;

  /* Variable-sized types get passed by reference, which can be passed
     in registers.  */
  if (bytes < 0)
    {
      if (asf <= 8 - (POINTER_SIZE / BITS_PER_WORD))
	*args_so_far += POINTER_SIZE / BITS_PER_WORD;

      return;
    }

  words = (bytes + UNITS_PER_WORD - 1) / UNITS_PER_WORD;

  /* Struct and unions which are not exactly the size of a register are to be
     passed on stack.  */
  if (mode == BLKmode
      && (TYPE_ALIGN (type) != BITS_PER_WORD || bytes != UNITS_PER_WORD))
    return;

  /* Align arguments requiring more than word alignment to a double-word
     boundary (or an even register number if the argument will get passed
     in registers).  */
  if ((asf & 1) != 0
      && (mode == DImode || mode == DFmode
	  || (type != NULL_TREE && TYPE_ALIGN (type) > BITS_PER_WORD)))
    asf++;

  if (asf + words > 8)
    return;

  (*args_so_far) = asf + words;
}

/* A C expression that indicates when an argument must be passed by
   reference.  If nonzero for an argument, a copy of that argument is
   made in memory and a pointer to the argument is passed instead of
   the argument itself.  The pointer is passed in whatever way is
   appropriate for passing a pointer to that type.

   On m88k, only variable sized types are passed by reference.  */

static bool
m88k_pass_by_reference (CUMULATIVE_ARGS *cum ATTRIBUTE_UNUSED,
			enum machine_mode mode ATTRIBUTE_UNUSED,
			tree type, bool named ATTRIBUTE_UNUSED)
{
  return type != NULL_TREE && int_size_in_bytes (type) < 0;
}

/* Disable the promotion of some structures and unions to registers.
   Note that this matches FUNCTION_ARG behaviour.  */
static bool
m88k_return_in_memory (tree type, tree fndecl ATTRIBUTE_UNUSED)
{
  switch (TYPE_MODE (type))
    {
      case BLKmode:
	return true;
      default:
	if ((TREE_CODE (type) == RECORD_TYPE || TREE_CODE (type) == UNION_TYPE)
	    && (TYPE_ALIGN (type) != BITS_PER_WORD
		|| GET_MODE_SIZE (TYPE_MODE (type)) != UNITS_PER_WORD))
	  return true;
	return false;
    }
}

/* Perform any needed actions needed for a function that is receiving a
   variable number of arguments.

   CUM is a variable of type CUMULATIVE_ARGS which gives info about
    the preceding args and about the function being called.

   MODE and TYPE are the mode and type of the current parameter.

   PRETEND_SIZE is a variable that should be set to the amount of stack
   that must be pushed by the prolog to pretend that our caller pushed
   it.

   Normally, this macro will push all remaining incoming registers on the
   stack and set PRETEND_SIZE to the length of the registers pushed.  */

void
m88k_setup_incoming_varargs (CUMULATIVE_ARGS *cum, enum machine_mode mode,
			     tree type, int *pretend_size, int no_rtl)
{
  CUMULATIVE_ARGS next_cum;
  tree fntype;
  int stdarg_p;
  int regcnt, delta;

  fntype = TREE_TYPE (current_function_decl);
  stdarg_p = (TYPE_ARG_TYPES (fntype) != NULL_TREE
	     && (TREE_VALUE (tree_last (TYPE_ARG_TYPES (fntype)))
		 != void_type_node));

  /* For varargs, we do not want to skip the dummy va_dcl argument.
     For stdargs, we do want to skip the last named argument.  */
  next_cum = *cum;
  if (stdarg_p)
    m88k_function_arg_advance(&next_cum, mode, type, 1);

  regcnt = next_cum < 8 ? 8 - next_cum : 0;
  delta = regcnt & 1;

  if (! no_rtl && regcnt != 0)
    {
      rtx mem, dst;
      int set, regno, offs;

      set = get_varargs_alias_set ();
      mem = gen_rtx_MEM (BLKmode,
			 plus_constant (virtual_incoming_args_rtx,
					- (regcnt + delta) * UNITS_PER_WORD));
      MEM_NOTRAP_P (mem) = 1;
      set_mem_alias_set (mem, set);

      /* Now store the incoming registers.  */
      /* The following is equivalent to
	 move_block_from_reg (2 + next_cum,
			      adjust_address (mem, Pmode,
					      delta * UNITS_PER_WORD),
			      regcnt, UNITS_PER_WORD * regcnt);
	 but using double store instruction since the stack is properly
	 aligned.  */
      regno = 2 + next_cum;
      dst = mem;

      if (delta != 0)
	{
	  dst = adjust_address (dst, Pmode, UNITS_PER_WORD);
	  emit_move_insn (operand_subword (dst, 0, 1, BLKmode),
			  gen_rtx_REG (SImode, regno));
	  regno++;
	}

      offs = delta;
      while (regno < 10)
	{
	  emit_move_insn (adjust_address (dst, DImode, offs * UNITS_PER_WORD),
			  gen_rtx_REG (DImode, regno));
	  offs += 2;
	  regno += 2;
        }

      *pretend_size = (regcnt + delta) * UNITS_PER_WORD;
    }
}

/* Define the `__builtin_va_list' type for the ABI.  */

static tree
m88k_build_va_list (void)
{
  tree field_reg, field_stk, field_arg, int_ptr_type_node, record;

  int_ptr_type_node = build_pointer_type (integer_type_node);

  record = make_node (RECORD_TYPE);

  field_arg = build_decl (FIELD_DECL, get_identifier ("__va_arg"),
			  integer_type_node);
  field_stk = build_decl (FIELD_DECL, get_identifier ("__va_stk"),
			  int_ptr_type_node);
  field_reg = build_decl (FIELD_DECL, get_identifier ("__va_reg"),
			  int_ptr_type_node);

  DECL_FIELD_CONTEXT (field_arg) = record;
  DECL_FIELD_CONTEXT (field_stk) = record;
  DECL_FIELD_CONTEXT (field_reg) = record;

  TYPE_FIELDS (record) = field_arg;
  TREE_CHAIN (field_arg) = field_stk;
  TREE_CHAIN (field_stk) = field_reg;

  layout_type (record);
  return record;
}

/* Implement `va_start' for varargs and stdarg.  */

void
m88k_va_start (tree valist, rtx nextarg ATTRIBUTE_UNUSED)
{
  tree field_reg, field_stk, field_arg;
  tree reg, stk, arg, t;
  tree fntype;
  int stdarg_p;
  int offset;

  gcc_assert (CONSTANT_P (current_function_arg_offset_rtx));

  field_arg = TYPE_FIELDS (va_list_type_node);
  field_stk = TREE_CHAIN (field_arg);
  field_reg = TREE_CHAIN (field_stk);

  arg = build3 (COMPONENT_REF, TREE_TYPE (field_arg), valist, field_arg,
		NULL_TREE);
  stk = build3 (COMPONENT_REF, TREE_TYPE (field_stk), valist, field_stk,
		NULL_TREE);
  reg = build3 (COMPONENT_REF, TREE_TYPE (field_reg), valist, field_reg,
		NULL_TREE);

  fntype = TREE_TYPE (current_function_decl);
  stdarg_p = (TYPE_ARG_TYPES (fntype) != NULL_TREE
	      && (TREE_VALUE (tree_last (TYPE_ARG_TYPES (fntype)))
		  != void_type_node));

  /* Fill in the __va_arg member.  */
  t = build2 (MODIFY_EXPR, TREE_TYPE (arg), arg,
	     size_int (current_function_args_info));
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  /* Store the arg pointer in the __va_stk member.  */
  offset = XINT (current_function_arg_offset_rtx, 0);
  if (current_function_args_info >= 8 && ! stdarg_p)
    offset -= UNITS_PER_WORD;
  t = make_tree (TREE_TYPE (stk), virtual_incoming_args_rtx);
  t = build2 (PLUS_EXPR, TREE_TYPE (stk), t, size_int (offset));
  t = build2 (MODIFY_EXPR, TREE_TYPE (stk), stk, t);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  /* Setup __va_reg */
  t = make_tree (TREE_TYPE (reg), virtual_incoming_args_rtx);
  t = build2 (PLUS_EXPR, TREE_TYPE (reg), t,
	     build_int_cst (NULL_TREE, -8 * UNITS_PER_WORD));
  t = build2 (MODIFY_EXPR, TREE_TYPE (reg), reg, t);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);
}

/* Implement `va_arg'.  */

tree
m88k_gimplify_va_arg (tree valist, tree type, tree *pre_p, tree *post_p)
{
  tree field_reg, field_stk, field_arg;
  int size, wsize, align;
  bool reg_p;
  tree ptrtype = build_pointer_type (type);
  tree lab_done;
  tree addr;
  tree t;

  if (pass_by_reference (NULL, TYPE_MODE (type), type, false))
    {
      t = m88k_gimplify_va_arg (valist, ptrtype, pre_p, post_p);
      return build_va_arg_indirect_ref (t);
    }

  field_arg = TYPE_FIELDS (va_list_type_node);
  field_stk = TREE_CHAIN (field_arg);
  field_reg = TREE_CHAIN (field_stk);

  size = int_size_in_bytes (type);
  wsize = (size + UNITS_PER_WORD - 1) / UNITS_PER_WORD;
  reg_p = (AGGREGATE_TYPE_P (type)
	   ? size == UNITS_PER_WORD && TYPE_ALIGN (type) == BITS_PER_WORD
	   : size <= 2*UNITS_PER_WORD);

  addr = create_tmp_var (ptr_type_node, "addr");
  DECL_POINTER_ALIAS_SET (addr) = get_varargs_alias_set ();
  lab_done = NULL;

  /* Decide if we should read from stack or regs if the argument could have
     been passed in registers.  */
  if (reg_p) {
    tree arg, arg_align, reg;
    tree lab_stack;
    tree u;

    lab_stack = create_artificial_label ();
    lab_done = create_artificial_label ();

    /* Align __va_arg to a doubleword boundary if necessary.  */
    arg = build3 (COMPONENT_REF, TREE_TYPE (field_arg), valist, field_arg,
		  NULL_TREE);
    align = type == NULL_TREE ? 0 : TYPE_ALIGN (type) / BITS_PER_WORD;
    if (align > 1)
      {
	t = build2 (PLUS_EXPR, TREE_TYPE (arg), arg, size_int (align - 1));
	arg_align = build2 (BIT_AND_EXPR, TREE_TYPE (t), t,
			    build_int_cst (NULL_TREE, -align));
	gimplify_expr (&arg_align, pre_p, NULL, is_gimple_val, fb_rvalue);
      }
    else
      arg_align = arg;

    /* Make sure the argument fits within the remainder of the saved
       register area, and branch to the stack logic if not.  */
    u = fold_convert (TREE_TYPE (arg), arg_align);
    /* if (arg_align > 8 - wsize) goto lab_stack */
    t = fold_convert (TREE_TYPE (arg), size_int (8 - wsize));
    t = build2 (GT_EXPR, boolean_type_node, u, t);
    u = build1 (GOTO_EXPR, void_type_node, lab_stack);
    t = build3 (COND_EXPR, void_type_node, t, u, NULL_TREE);
    gimplify_and_add (t, pre_p);

    /* Compute the argument address.  */
    reg = build3 (COMPONENT_REF, TREE_TYPE (field_reg), valist, field_reg,
		  NULL_TREE);
    t = build2 (MULT_EXPR, TREE_TYPE (reg), arg_align,
	       size_int (UNITS_PER_WORD));
    t = build2 (PLUS_EXPR, TREE_TYPE (reg), reg, t);

    t = build2 (MODIFY_EXPR, void_type_node, addr, t);
    gimplify_and_add (t, pre_p);

    /* Increment __va_arg.  */
    t = build2 (PLUS_EXPR, TREE_TYPE (arg), arg_align, size_int (wsize));
    t = build2 (MODIFY_EXPR, TREE_TYPE (arg), arg, t);
    gimplify_and_add (t, pre_p);

    t = build1 (GOTO_EXPR, void_type_node, lab_done);
    gimplify_and_add (t, pre_p);

    t = build1 (LABEL_EXPR, void_type_node, lab_stack);
    append_to_statement_list (t, pre_p);
  }

  {
    tree stk;
    tree u;

    stk = build3 (COMPONENT_REF, TREE_TYPE (field_stk), valist, field_stk,
		  NULL_TREE);

    /* Align __va_stk to the type boundary if necessary.  */
    align = type == NULL_TREE ? 0 : TYPE_ALIGN (type) / BITS_PER_UNIT;
    if (align > UNITS_PER_WORD)
      {
        t = build2 (PLUS_EXPR, TREE_TYPE (stk), stk, size_int (align - 1));
        t = build2 (BIT_AND_EXPR, TREE_TYPE (t), t,
		    build_int_cst (NULL_TREE, -align));
	gimplify_expr (&t, pre_p, NULL, is_gimple_val, fb_rvalue);
      }
    else
      t = stk;

    /* Compute the argument address.  */
    u = build2 (MODIFY_EXPR, void_type_node, addr, t);
    gimplify_and_add (u, pre_p);

    /* Increment __va_stk.  */
    t = build2 (PLUS_EXPR, TREE_TYPE (t), t, size_int (wsize * UNITS_PER_WORD));
    t = build2 (MODIFY_EXPR, TREE_TYPE (stk), stk, t);
    gimplify_and_add (t, pre_p);
  }

  if (lab_done)
    {
      t = build1 (LABEL_EXPR, void_type_node, lab_done);
      append_to_statement_list (t, pre_p);
    }

  addr = fold_convert (ptrtype, addr);
  return build_va_arg_indirect_ref (addr);
}

/* If cmpsi has not been generated, emit code to do the test.  Return the
   expression describing the test of operator OP.  */

rtx
emit_test (enum rtx_code op, enum machine_mode mode)
{
  if (m88k_compare_reg == NULL_RTX)
    emit_insn (gen_test (m88k_compare_op0, m88k_compare_op1));
  return (gen_rtx_fmt_ee (op, mode, m88k_compare_reg, const0_rtx));
}

/* Determine how to best perform cmpsi/bxx, where cmpsi has a constant
   operand.  All tests with zero (albeit swapped) and all equality tests
   with a constant are done with bcnd.  The remaining cases are swapped
   as needed.  */

void
emit_bcnd (enum rtx_code op, rtx label)
{
  if (m88k_compare_op1 == const0_rtx)
    emit_jump_insn (gen_bcnd
		    (gen_rtx_fmt_ee (op, VOIDmode, m88k_compare_op0, const0_rtx),
		     label));
  else if (m88k_compare_op0 == const0_rtx)
    emit_jump_insn (gen_bcnd
		    (gen_rtx_fmt_ee (swap_condition (op),
				     VOIDmode, m88k_compare_op1, const0_rtx),
		     label));
  else if (op != EQ && op != NE)
    emit_jump_insn (gen_bxx (emit_test (op, VOIDmode), label));
  else
    {
      rtx zero = gen_reg_rtx (SImode);
      rtx reg, constant;
      int value;

      if (GET_CODE (m88k_compare_op1) == CONST_INT)
	{
	  reg = force_reg (SImode, m88k_compare_op0);
	  constant = m88k_compare_op1;
	}
      else
	{
	  reg = force_reg (SImode, m88k_compare_op1);
	  constant = m88k_compare_op0;
	}
      value = INTVAL (constant);

      /* Perform an arithmetic computation to make the compared-to value
	 zero, but avoid loosing if the bcnd is later changed into sxx.  */
      if (SMALL_INTVAL (value))
	emit_jump_insn (gen_bxx (emit_test (op, VOIDmode), label));
      else
	{
	  if (SMALL_INTVAL (-value))
	    emit_insn (gen_addsi3 (zero, reg,
				   GEN_INT (-value)));
	  else
	    emit_insn (gen_xorsi3 (zero, reg, constant));

	  emit_jump_insn (gen_bcnd (gen_rtx_fmt_ee (op, VOIDmode,
						    zero, const0_rtx),
				    label));
	}
    }
}

/* Print an operand.  Recognize special options, documented below.  */

void
print_operand (FILE *file, rtx x, int code)
{
  enum rtx_code xc = (x ? GET_CODE (x) : UNKNOWN);
  int value = (xc == CONST_INT ? INTVAL (x) : 0);
  static int sequencep;
  static int reversep;

  if (sequencep)
    {
      if (code < 'B' || code > 'E')
	output_operand_lossage ("%%R not followed by %%B/C/D/E");
      if (reversep)
	xc = reverse_condition (xc);
      sequencep = 0;
    }

  switch (code)
    {
    case '#': /* register prefix character (may be empty) */
      fputs (m88k_register_prefix, file);
      return;

    case 'V': /* Output a serializing instruction as needed if the operand
		 (assumed to be a MEM) is a volatile load.  */
    case 'v': /* ditto for a volatile store.  */
      if (MEM_VOLATILE_P (x) && TARGET_SERIALIZE_VOLATILE)
	{
	  /* The m88110 implements two FIFO queues, one for loads and
	     one for stores.  These queues mean that loads complete in
	     their issue order as do stores.  An interaction between the
	     history buffer and the store reservation station ensures
	     that a store will not bypass load.  Finally, a load will not
	     bypass store, but only when they reference the same address.

	     To avoid this reordering (a load bypassing a store) for
	     volatile references, a serializing instruction is output.
	     We choose the fldcr instruction as it does not serialize on
	     the m88100 so that -m88000 code will not be degraded.

	     The mechanism below is completed by having CC_STATUS_INIT set
	     the code to the unknown value.  */

	  /*
	     hassey 6/30/93
	     A problem with 88110 4.1 & 4.2 makes the use of fldcr for
	     this purpose undesirable.  Instead we will use tb1, this will
	     cause serialization on the 88100 but such is life.
	  */

	  static rtx last_addr = NULL_RTX;
	  if (code == 'V' /* Only need to serialize before a load.  */
	      && m88k_volatile_code != 'V' /* Loads complete in FIFO order.  */
	      && !(m88k_volatile_code == 'v'
		   && GET_CODE (XEXP (x, 0)) == LO_SUM
		   && rtx_equal_p (XEXP (XEXP (x, 0), 1), last_addr)))
	    asm_fprintf (file,
#if 0
			 "fldcr\t %R%s,%Rfcr63\n\t",
#else /* 0 */
			 "tb1\t 1,%R%s,0xff\n\t",
#endif /* 0 */
			 reg_names[0]);
	  m88k_volatile_code = code;
	  last_addr = (GET_CODE (XEXP (x, 0)) == LO_SUM
		       ? XEXP (XEXP (x, 0), 1) : 0);
	}
      return;

    case 'X': /* print the upper 16 bits... */
      value >>= 16;
    case 'x': /* print the lower 16 bits of the integer constant in hex */
      if (xc != CONST_INT)
	output_operand_lossage ("invalid %%x/X value");
      fprintf (file, "0x%x", value & 0xffff); return;

    case 'H': /* print the low 16 bits of the negated integer constant */
      if (xc != CONST_INT)
	output_operand_lossage ("invalid %%H value");
      value = -value;
    case 'h': /* print the register or low 16 bits of the integer constant */
      if (xc == REG)
	goto reg;
      if (xc != CONST_INT)
	output_operand_lossage ("invalid %%h value");
      fprintf (file, "%d", value & 0xffff);
      return;

    case 'Q': /* print the low 8 bits of the negated integer constant */
      if (xc != CONST_INT)
	output_operand_lossage ("invalid %%Q value");
      value = -value;
    case 'q': /* print the register or low 8 bits of the integer constant */
      if (xc == REG)
	goto reg;
      if (xc != CONST_INT)
	output_operand_lossage ("invalid %%q value");
      fprintf (file, "%d", value & 0xff);
      return;

    case 'p': /* print the logarithm of the integer constant */
      if (xc != CONST_INT
	  || (value = exact_log2 (value)) < 0)
	output_operand_lossage ("invalid %%p value");
      fprintf (file, "%d", value);
      return;

    case 'S': /* complement the value and then... */
      value = ~value;
    case 's': /* print the width and offset values forming the integer
		 constant with a SET instruction.  See integer_ok_for_set. */
      {
	unsigned mask, uval = value;
	int top, bottom;

	if (xc != CONST_INT)
	  output_operand_lossage ("invalid %%s/S value");
	/* All the "one" bits must be contiguous.  If so, MASK will be
	   a power of two or zero.  */
	mask = (uval | (uval - 1)) + 1;
	if (!(uval && POWER_OF_2_or_0 (mask)))
	  output_operand_lossage ("invalid %%s/S value");
	top = mask ? exact_log2 (mask) : 32;
	bottom = exact_log2 (uval & ~(uval - 1));
	fprintf (file,"%d<%d>", top - bottom, bottom);
	return;
      }

    case 'P': /* print nothing if pc_rtx; output label_ref */
      if (xc == LABEL_REF)
	output_addr_const (file, x);
      else if (xc != PC)
	output_operand_lossage ("invalid %%P operand");
      return;

    case 'L': /* print 0 or 1 if operand is label_ref and then...  */
      fputc (xc == LABEL_REF ? '1' : '0', file);
    case '.': /* print .n if delay slot is used */
      fputs ((final_sequence
	      && ! INSN_ANNULLED_BRANCH_P (XVECEXP (final_sequence, 0, 0)))
	     ? ".n\t" : "\t", file);
      return;

    case '!': /* Reverse the following condition. */
      sequencep++;
      reversep = 1;
      return; 
    case 'R': /* reverse the condition of the next print_operand
		 if operand is a label_ref.  */
      sequencep++;
      reversep = (xc == LABEL_REF);
      return;

    case 'B': /* bcnd branch values */
      if (0) /* SVR4 */
	fputs (m88k_register_prefix, file);
      switch (xc)
	{
	case EQ: fputs ("eq0", file); return;
	case NE: fputs ("ne0", file); return;
	case GT: fputs ("gt0", file); return;
	case LE: fputs ("le0", file); return;
	case LT: fputs ("lt0", file); return;
	case GE: fputs ("ge0", file); return;
	default: output_operand_lossage ("invalid %%B value");
	}

    case 'C': /* bb0/bb1 branch values for comparisons */
      if (0) /* SVR4 */
	fputs (m88k_register_prefix, file);
      switch (xc)
	{
	case EQ:  fputs ("eq", file); return;
	case NE:  fputs ("ne", file); return;
	case GT:  fputs ("gt", file); return;
	case LE:  fputs ("le", file); return;
	case LT:  fputs ("lt", file); return;
	case GE:  fputs ("ge", file); return;
	case GTU: fputs ("hi", file); return;
	case LEU: fputs ("ls", file); return;
	case LTU: fputs ("lo", file); return;
	case GEU: fputs ("hs", file); return;
	default:  output_operand_lossage ("invalid %%C value");
	}

    case 'D': /* bcnd branch values for float comparisons */
      switch (xc)
	{
	case EQ: fputs ("0xa", file); return;
	case NE: fputs ("0x5", file); return;
	case GT:
	  if (0) /* SVR4 */
	    fputs (m88k_register_prefix, file);
	  fputs ("gt0", file);
	  return;
	case LE:
	  if (0) /* SVR4 */
	    fputs (m88k_register_prefix, file);
	  fputs ("le0", file);
	  return;
	case LT: fputs ("0x4", file); return;
	case GE: fputs ("0xb", file); return;
	default: output_operand_lossage ("invalid %%D value");
	}

    case 'E': /* bcnd branch values for special integers */
      switch (xc)
	{
	case EQ: fputs ("0x8", file); return;
	case NE: fputs ("0x7", file); return;
	default: output_operand_lossage ("invalid %%E value");
	}

    case 'd': /* second register of a two register pair */
      if (xc != REG)
	output_operand_lossage ("`%%d' operand isn't a register");
      asm_fprintf (file, "%R%s", reg_names[REGNO (x) + 1]);
      return;

    case 'r': /* an immediate 0 should be represented as `r0' */
      if (x == const0_rtx)
	{
	  asm_fprintf (file, "%R%s", reg_names[0]);
	  return;
	}
      else if (xc != REG)
	output_operand_lossage ("invalid %%r value");
    case 0:
    name:
      if (xc == REG)
	{
	reg:
	  if (REGNO (x) == ARG_POINTER_REGNUM)
	    output_operand_lossage ("operand is r0");
	  else
	    asm_fprintf (file, "%R%s", reg_names[REGNO (x)]);
	}
      else if (xc == PLUS)
	output_address (x);
      else if (xc == MEM)
	output_address (XEXP (x, 0));
      else if (flag_pic && xc == UNSPEC)
	{
	  output_addr_const (file, XVECEXP (x, 0, 0));
	  fputs ("#got_rel", file);
	}
      else if (xc == CONST_DOUBLE)
	output_operand_lossage ("operand is const_double");
      else
	output_addr_const (file, x);
      return;

    case 'g': /* append #got_rel as needed */
      if (flag_pic && (xc == SYMBOL_REF || xc == LABEL_REF))
	{
	  output_addr_const (file, x);
	  fputs ("#got_rel", file);
	  return;
	}
      goto name;

    case 'a': /* (standard), assume operand is an address */
    case 'c': /* (standard), assume operand is an immediate value */
    case 'l': /* (standard), assume operand is a label_ref */
    case 'n': /* (standard), like %c, except negate first */
    default:
      output_operand_lossage ("invalid code");
    }
}

void
print_operand_address (FILE *file, rtx addr)
{
  rtx reg0, reg1;

  switch (GET_CODE (addr))
    {
    case REG:
      gcc_assert (REGNO (addr) != ARG_POINTER_REGNUM);
      asm_fprintf (file, "%R%s,%R%s", reg_names[0], reg_names [REGNO (addr)]);
      break;

    case LO_SUM:
      asm_fprintf (file, "%R%s,%Rlo16(",
		   reg_names[REGNO (XEXP (addr, 0))]);
      output_addr_const (file, XEXP (addr, 1));
      fputc (')', file);
      break;

    case PLUS:
      reg0 = XEXP (addr, 0);
      reg1 = XEXP (addr, 1);
      if (GET_CODE (reg0) == MULT || GET_CODE (reg0) == CONST_INT)
	{
	  rtx tmp = reg0;
	  reg0 = reg1;
	  reg1 = tmp;
	}

      gcc_assert ((!REG_P (reg0) || REGNO (reg0) != ARG_POINTER_REGNUM)
		  && (!REG_P (reg1) || REGNO (reg1) != ARG_POINTER_REGNUM));

      if (REG_P (reg0))
	{
	  if (REG_P (reg1))
	    asm_fprintf (file, "%R%s,%R%s",
			 reg_names [REGNO (reg0)], reg_names [REGNO (reg1)]);

	  else if (GET_CODE (reg1) == CONST_INT)
	    asm_fprintf (file, "%R%s,%d",
			 reg_names [REGNO (reg0)], INTVAL (reg1));

	  else if (GET_CODE (reg1) == MULT)
	    {
	      rtx mreg = XEXP (reg1, 0);
	      gcc_assert (REGNO (mreg) != ARG_POINTER_REGNUM);

	      asm_fprintf (file, "%R%s[%R%s]", reg_names[REGNO (reg0)],
			   reg_names[REGNO (mreg)]);
	    }

	  else if (GET_CODE (reg1) == ZERO_EXTRACT)
	    {
	      asm_fprintf (file, "%R%s,%Rlo16(",
			   reg_names[REGNO (reg0)]);
	      output_addr_const (file, XEXP (reg1, 0));
	      fputc (')', file);
	    }

	  else if (flag_pic)
	    {
	      asm_fprintf (file, "%R%s,", reg_names[REGNO (reg0)]);
	      output_addr_const (file, reg1);
	      fputs ("#got_rel", file);
	    }
	  else
	    gcc_unreachable ();
	}

      else
	gcc_unreachable ();
      break;

    case MULT:
      gcc_assert (REGNO (XEXP (addr, 0)) != ARG_POINTER_REGNUM);
      asm_fprintf (file, "%R%s[%R%s]",
		   reg_names[0], reg_names[REGNO (XEXP (addr, 0))]);
      break;

    case CONST_INT:
      asm_fprintf (file, "%R%s,%d", reg_names[0], INTVAL (addr));
      break;

    default:
      asm_fprintf (file, "%R%s,", reg_names[0]);
      output_addr_const (file, addr);
    }
}

/* Return true if X is an address which needs a temporary register when 
   reloaded while generating PIC code.  */

bool
pic_address_needs_scratch (rtx x)
{
  /* An address which is a symbolic plus a non SMALL_INT needs a temp reg.  */
  if (GET_CODE (x) == CONST)
    {
      x = XEXP (x, 0);
      if (GET_CODE (x) == PLUS)
	{
	  if (GET_CODE (XEXP (x, 0)) == SYMBOL_REF
	      && GET_CODE (XEXP (x, 1)) == CONST_INT
	      && ! ADD_INT (XEXP (x, 1)))
	    return true;
	}
    }

  return false;
}

/* Adjust the cost of INSN based on the relationship between INSN that
   is dependent on DEP_INSN through the dependence LINK.  The default
   is to make no adjustment to COST.

   On the m88k, ignore the cost of anti- and output-dependencies.  On
   the m88100, a store can issue two cycles before the value (not the
   address) has finished computing.  */

static int
m88k_adjust_cost (rtx insn, rtx link, rtx dep, int cost)
{
  if (REG_NOTE_KIND (link) != REG_DEP_TRUE)
    return 0;  /* Anti or output dependence.  */

  if (TARGET_88110
      && recog_memoized (insn) >= 0
      && get_attr_type (insn) == TYPE_STORE
      && SET_SRC (PATTERN (insn)) == SET_DEST (PATTERN (dep)))
    return cost - 4;  /* 88110 store reservation station.  */

  return cost;
}

/* Return cost of address expression X.
   Expect that X is properly formed address reference.  */

static int
m88k_address_cost (rtx x)
{
   /* REG+REG is made slightly more expensive because it might keep
      a register live for longer than we might like.  */
  switch (GET_CODE (x))
    {
    case REG:
    case LO_SUM:
    case MULT:
      return 1;
    case HIGH:
      return 2;
    case PLUS:
      return (REG_P (XEXP (x, 0)) && REG_P (XEXP (x, 1))) ? 2 : 1;
    default:
      return 4;
    }
}

/* Compute the cost of computing a constant rtl expression x
   whose rtx-code is code.  */
static bool
m88k_rtx_costs (rtx x, int code, int outer_code, int *total)
{
  switch (code)
    {
    case CONST_INT:
      /* We assume that any 16 bit integer can easily be recreated, so we
	 indicate 0 cost, in an attempt to get GCC not to optimize things
	 like comparison against a constant.  */
      if (SMALL_INT (x))
        *total = 0;
      else if (SMALL_INTVAL (- INTVAL (x)))
        *total = 2;
      else if (classify_integer (SImode, INTVAL (x)) != m88k_oru_or)
        *total = 4;
      else
	*total = 7;
      return true;

    case HIGH:
      *total = 2;
      return true;

    case CONST:
    case LABEL_REF:
    case SYMBOL_REF:
      if (flag_pic)
        *total = (flag_pic == 2) ? 11 : 8;
      else
        *total = 5;
      return true;

      /* The cost of CONST_DOUBLE is zero (if it can be placed in an insn,
	 it is as good as a register; since it can't be placed in any insn,
	 it won't do anything in cse, but it will cause expand_binop to
	 pass the constant to the define_expands).  */
    case CONST_DOUBLE:
      *total = 0;
      return true;

    case MEM:
      *total = COSTS_N_INSNS (2);
      return true;

    case MULT:
      *total = COSTS_N_INSNS (3);
      return true;

    case DIV:
    case UDIV:
    case MOD:
    case UMOD:
      *total = COSTS_N_INSNS (38);
      return true;

    default:
      return false;
    }
}

static bool
m88k_handle_option (size_t code, const char *arg, int value)
{
  switch (code)
    {
    case OPT_m88000:
      /* make the cpu type nonzero; will be reset in m88k_override_options() */
      target_flags |= MASK_88100 | MASK_88110;
      return true;

    case OPT_m88100:
      target_flags &= ~MASK_88110;
      target_flags |= MASK_88100;
      return true;

    case OPT_m88110:
      target_flags &= ~MASK_88100;
      target_flags |= MASK_88110;
      return true;

    default:
      return true;
    }
}

void
m88k_override_options (void)
{
  if (!TARGET_88100 && !TARGET_88110)
    target_flags |= CPU_DEFAULT;

  if (TARGET_88100 && TARGET_88110)
    target_flags &= ~(MASK_88100 | MASK_88110);

  if (TARGET_88110)
    {
      target_flags |= MASK_USE_DIV;
      target_flags &= ~MASK_CHECK_ZERO_DIV;
    }

  m88k_cpu = (TARGET_88110 ? PROCESSOR_M88100
	      : (TARGET_88100 ? PROCESSOR_M88100 : PROCESSOR_M88000));

  if (TARGET_TRAP_LARGE_SHIFT && TARGET_HANDLE_LARGE_SHIFT)
    error ("-mtrap-large-shift and -mhandle-large-shift are incompatible");

  if (TARGET_OMIT_LEAF_FRAME_POINTER)	/* keep nonleaf frame pointers */
    flag_omit_frame_pointer = 1;

  /* On the m88100, it is desirable to align functions to a cache line.
     The m88110 cache is small, so align to an 8 byte boundary.  */
  if (align_functions == 0)
    align_functions = TARGET_88100 ? 16 : 8;

#if 1 /* XXX breaks -freorder-blocks and even without it, tree-cfg.c */
  flag_delayed_branch = 0;
#endif

  /* XXX -freorder-blocks (enabled at -O2) does not work with -fdelayed-branch
     yet.  */
  if (flag_delayed_branch)
    {
      flag_reorder_blocks = flag_reorder_blocks_and_partition = 0;
    }
}
