#include "common.h"
#include "vtasm.h"
#include "vtsystem.h"

NTSTATUS InitializeSegmentSelector( PSEGMENT_SELECTOR SegmentSelector, USHORT Selector, ULONG GdtBase )
{
	PSEGMENT_DESCRIPTOR2 SegDesc;

	if( !SegmentSelector )
	{
		return STATUS_INVALID_PARAMETER;
	}

	//
	// �����ѡ���ӵ�T1 = 1��ʾ����LDT�е���, ����û��ʵ���������
	//
	if( Selector & 0x4 )
	{
		
		return STATUS_INVALID_PARAMETER;
	}

	//
	// ��GDT��ȡ��ԭʼ�Ķ�������
	//
	SegDesc = ( PSEGMENT_DESCRIPTOR2 )( ( PUCHAR ) GdtBase + ( Selector & ~0x7 ) );

	//
	// ��ѡ����
	//
	SegmentSelector->sel = Selector;

	//
	// �λ�ַ15-39λ 55-63λ
	//
	SegmentSelector->base = SegDesc->base0 | SegDesc->base1 << 16 | SegDesc->base2 << 24;

	//
	// ���޳�0-15λ  47-51λ, ������ȡ��
	//
	SegmentSelector->limit = SegDesc->limit0 | ( SegDesc->limit1attr1 & 0xf ) << 16;

	//
	// ������39-47 51-55 ע��۲�ȡ��
	//
	SegmentSelector->attributes.UCHARs = SegDesc->attr0 | ( SegDesc->limit1attr1 & 0xf0 ) << 4;

	//
	// �����ж����Ե�DTλ, �ж��Ƿ���ϵͳ�����������Ǵ������ݶ�������
	//
	if( !( SegDesc->attr0 & LA_STANDARD ) )
	{
		ULONG64 tmp;

		//
		// �����ʾ��ϵͳ��������������������, �о�����Ϊ64λ׼���İ�,
		// 32λ����λ�ַֻ��32λ��. �ѵ�64λ������ʲô������?
		//
		tmp = ( *( PULONG64 )( ( PUCHAR ) SegDesc + 8 ) );

		SegmentSelector->base = ( SegmentSelector->base & 0xffffffff ) | ( tmp << 32 );
	}

	//
	// ���Ƕν��޵�����λ, 1Ϊ4K. 0Ϊ1BYTE
	//
	if( SegmentSelector->attributes.fields.g )
	{
		//
		// �������λΪ1, ��ô�ͳ���4K. ���ƶ�12λ
		//
		SegmentSelector->limit = ( SegmentSelector->limit << 12 ) + 0xfff;
	}

	return STATUS_SUCCESS;
}

ULONG NTAPI VmxAdjustControls(
								ULONG Ctl,
								ULONG Msr
								)
{
	LARGE_INTEGER MsrValue;

	MsrValue.QuadPart = Asm_ReadMsr(Msr);
	Ctl &= MsrValue.HighPart;     /* bit == 0 in high word ==> must be zero */
	Ctl |= MsrValue.LowPart;      /* bit == 1 in low word  ==> must be one  */
	return Ctl;
}

VOID SetBit( ULONG * dword, ULONG bit )
{
	ULONG mask = ( 1 << bit );
	*dword = *dword | mask;
}

VOID ClearBit(ULONG * dword, ULONG bit)
{
	ULONG mask = 0xFFFFFFFF;
	ULONG sub = (1 << bit);
	mask = mask - sub;
	*dword = *dword & mask;
}

//VOID ClearBit(VOID * dword_, ULONG bit)
//{
//	ULONG mask = 0xFFFFFFFF;
//	ULONG sub = (1 << bit);
//	mask = mask - sub;
//	ULONG * dword = (ULONG*)dword_;
//	*dword = *dword & mask;
//}

NTSTATUS FillGuestSelectorData(ULONG GdtBase, ULONG Segreg, USHORT
							   Selector)
{
	SEGMENT_SELECTOR SegmentSelector = { 0 };
	ULONG uAccessRights;

	InitializeSegmentSelector(&SegmentSelector, Selector, GdtBase);
	uAccessRights = ((PUCHAR)& SegmentSelector.attributes)[0] + (((PUCHAR)&
		SegmentSelector.attributes)[1] << 12);

	if (!Selector)
		uAccessRights |= 0x10000;

	Vmx_VmWrite(GUEST_ES_SELECTOR + Segreg * 2, Selector & 0xFFF8);
	Vmx_VmWrite(GUEST_ES_BASE + Segreg * 2, SegmentSelector.base);
	Vmx_VmWrite(GUEST_ES_LIMIT + Segreg * 2, SegmentSelector.limit);
	Vmx_VmWrite(GUEST_ES_AR_BYTES + Segreg * 2, uAccessRights);


// 	if ((Segreg == LDTR) || (Segreg == TR))
// 		// don't setup for FS/GS - their bases are stored in MSR values
// 		Vmx_VmWrite(GUEST_ES_BASE + Segreg * 2, SegmentSelector.base);

	return STATUS_SUCCESS;
}