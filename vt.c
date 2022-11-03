#include <immintrin.h>
#define unlikely(x) __builtin_expect(x, 0)

#define ESC "\x1b"
#define CSI ESC "["
#define SS3 ESC "O"

typedef enum {
	InputState_Normal,
	InputState_Exit,
	InputState_Escape,
	InputState_Escape_VT52,
	InputState_Esc_SPC,
	InputState_Esc_Hash,
	InputState_Esc_Pct,
	InputState_SelectCharset,
	InputState_CSI,
	InputState_CSI_priv,
	InputState_CSI_Quote,
	InputState_CSI_DblQuote,
	InputState_CSI_Bang,
	InputState_CSI_SPC,
	InputState_CSI_GT,
	InputState_DCS,
	InputState_DCS_Esc,
	InputState_OSC,
	InputState_OSC_Esc,
	InputState_VT52_CUP_Arg1,
	InputState_VT52_CUP_Arg2
} InputState;

static char unsigned OverhangMask[32] =
{
    255, 255, 255, 255,  255, 255, 255, 255,  255, 255, 255, 255,  255, 255, 255, 255,
    0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0
};

void ProcessChar(Terminal *t, InputState *state, char ch) {
	switch (*state) {
		case InputState_Exit:
			if (ch != '\e') *state = InputState_Normal;
			// fallthrough
		case InputState_Normal: {
			switch (ch) {
				case '\e':
					*state = InputState_Escape;
					break;
				default:
					// TODO(ym);
					break;
			}
		} break;
		case InputState_Escape: {
			switch (ch) {
				case '\x18': case '\x1a':
					*state = InputState_Exit;
					break;
				case '\e':
					*state = InputState_Escape;
					break;
				case '[':
					*state = InputState_CSI;
					break;
				case ']':
					*state = InputState_OSC;
					break;
			} break;
		} break;
	}
	return state;
}


void ProcessInputNormal(Terminal *t, InputState *state source_buffer_range range) {
	__m128i Carriage = _mm_set1_epi8('\n');
	__m128i Escape = _mm_set1_epi8('\x1b');
	__m128i Comp = _mm_set1_epi8(0x80);

	while (range.Count) {
		if (*state == InputState_Normal) {
			__m128i ContainsComplex = _mm_setzero_si128();
			int nTestE;
			int Check = 0;
			while (range.Count >= 16) {
				__m128i batch = _mm_loadu_si128((__m128i *) range.Data);
				__m128i TestC = _mm_cmpeq_epi8(batch, Carriage);
				__m128i TestE = _mm_cmpeq_epi8(batch, Escape);
				__m128i TestX = _mm_cmpeq_epi8(batch, Comp); // Not really needed
				__m128i Test = _mm_or_si128(TestC, TestE);

				/* nTestE = _mm_movemask_epi8(nTestE); */
				Check = _mm_movemask_epi8(TestE);

				if (unlikely(Check)) {
					int Advance = _tzcnt_u32(Check);

					__m128i Mask = _mm_loadu_si128((__m128i *)(OverhangMask + 16 - Advance));
					TestX = _mm_and_si128(TestX, Mask);
					ContainsComplex = _mm_or_si128(ContainsComplex, TestX);

					TestC = _mm_and_si128(TestC, Mask);
					int nTestC = _mm_movemask_epi8(TestC);
					// += __popcnt(nTestC); // TODO(YM):
					range = ConsumeCount(range, Advance);
					*state = InputState_Escape;
					break;
				}

				int nTestC = _mm_movemask_epi8(TestC);
				// += __popcnt(nTestC); // TODO(YM):

				ContainsComplex = _mm_or_si128(ContainsComplex, TestX);
				range = ConsumeCount(range, 16);
			}
		}

		// Handling escape codes,  leftover input
		// InputState_Exit doesn't feel that elegent, Is it really worth it?
		InputState prev_state = *state;
		while (range.Count && (*state == prev_state || state == InputState_Exit)) {
			ProcessChar(t, &state, PeekToken(range, 0));
			range = ConsumeCount(range, 1);
		}
	}
}

void ProcessInputEmulated(Terminal *t, source_buffer_range ob) {
}
