/**
 * COPYRIGHT (C)  SAMSUNG Electronics CO., LTD (Suwon, Korea). 2009
 * All rights are reserved. Reproduction and redistiribution in whole or
 * in part is prohibited without the written consent of the copyright owner.
 */

/**
 * xprint - print utility
 * 
 * @author kumhyun.cho@samsung.com
 * @since 2011.04.19
 */

#pragma once

#ifndef xstr
#define xstr(x) #x
#endif

#ifdef xprint0

#define xprint(fmt, args...) \
    xprint0("%s():%d: " fmt "\n", __func__, __LINE__, ##args)
#else
#define xprint
#endif

#define sprint(str) \
	xprint("%s", str)

#define vreturn(rst, fmt, args...) \
{ \
	xprint("return " fmt, rst, ##args); \
	return (rst); \
}

#define sreturn(rst, str) \
	vreturn(rst, "%d, %s", str)

#define xreturn(rst) \
	sreturn(rst, "")

#define vstart(exp, rst, fmt, args...) \
	if (exp) vreturn(rst, fmt, ##args);
 
#define xstart(exp, rst) \
	if (exp) vreturn(rst, "invalid param (" #exp ")");

#define sbreak(str) \
{ \
	xprint("break, %s", str); \
	break; \
}

#define xbreak() \
	sbreak("");
 
#ifndef xfree0
#define xfree0 free
#endif

#define xfree(p) \
	if (p) { \
		xfree0(p); \
		p = NULL; \
	}

#define vstmt(stmt) \
	xprint("" #stmt "")

#define xstmt(stmt, type, fmt) \
	({type r = (stmt); xprint("" #stmt "=" #fmt, r); r;})

#define dstmt(stmt) xstmt(stmt, int, "%d")
#define pstmt(stmt) xstmt(stmt, void*, "0x%p")
