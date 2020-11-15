/*
 * Copyright (C) 2018 alx@fastestcode.org
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "WmGlobal.h"
#include "WmError.h"
#include "WmImage.h"
#include "WmCDInfo.h"
#include "WmCDecor.h"
#include "WmXinerama.h"
#include "WmEwmh.h"

#ifdef DEBUG
#define dbg_printf(...) fprintf(stderr,__VA_ARGS__)
#else
#define dbg_printf(...) ((void)0)
#endif

static void* FetchWindowProperty(Window wnd, Atom prop,
	Atom req_type, unsigned long *size);
static Pixmap GetEwmhIconPixmap(const ClientData *pCD);

enum ewmh_atom {
	_NET_SUPPORTED, _NET_SUPPORTING_WM_CHECK,
	_NET_REQUEST_FRAME_EXTENTS, _NET_WM_NAME, 
	_NET_WM_ICON_NAME, _NET_WM_STATE, _NET_WM_STATE_FULLSCREEN,
	_NET_WM_ICON, _NET_FRAME_EXTENTS, _NET_ACTIVE_WINDOW,
	
	_NUM_EWMH_ATOMS
};

/* These must be in sync with enum ewmh_atom */
static char *ewmh_atom_names[_NUM_EWMH_ATOMS]={
	"_NET_SUPPORTED", "_NET_SUPPORTING_WM_CHECK",
	"_NET_REQUEST_FRAME_EXTENTS", "_NET_WM_NAME",
	 "_NET_WM_ICON_NAME", "_NET_WM_STATE", "_NET_WM_STATE_FULLSCREEN",
	"_NET_WM_ICON", "_NET_FRAME_EXTENTS", "_NET_ACTIVE_WINDOW"
};

/* Initialized in SetupEwhm() */
static Atom ewmh_atoms[_NUM_EWMH_ATOMS];
static Atom XA_UTF8_STRING;

/*
 * Initializes data and sets up root window properties for EWMH support.
 */
void SetupWmEwmh(void)
{
	int i;

	XInternAtoms(DISPLAY,ewmh_atom_names,_NUM_EWMH_ATOMS,False,ewmh_atoms);
	XA_UTF8_STRING = XInternAtom(DISPLAY,"UTF8_STRING",False);

	/* Add root properties indicating what EWMH protocols we support */
	for(i = 0; i < wmGD.numScreens; i++){
		Window check_wnd = XtWindow(wmGD.Screens[i].screenTopLevelW);
		
		XChangeProperty(DISPLAY,wmGD.Screens[i].rootWindow,
			ewmh_atoms[_NET_SUPPORTED],XA_ATOM,32,PropModeReplace,
			(unsigned char*)&ewmh_atoms[1],	_NUM_EWMH_ATOMS-1);
	
		/* Set up the device for _NET_SUPPORTING_WM_CHECK */
		XChangeProperty(DISPLAY,wmGD.Screens[i].rootWindow,
			ewmh_atoms[_NET_SUPPORTING_WM_CHECK],XA_WINDOW,32,PropModeReplace,
			(unsigned char*)&check_wnd,1);

		XChangeProperty(DISPLAY,check_wnd,
			ewmh_atoms[_NET_SUPPORTING_WM_CHECK],XA_WINDOW,32,PropModeReplace,
			(unsigned char*)&check_wnd,1);
			
		XChangeProperty(DISPLAY,check_wnd,ewmh_atoms[_NET_WM_NAME],
			XA_STRING,8,PropModeReplace,(unsigned char*)WM_RESOURCE_NAME,
			strlen(WM_RESOURCE_NAME));
	}
}

/*
 * Sets the _NET_FRAME_EXTENTS property on client
 */
void UpdateEwmhFrameExtents(ClientData *pCD)
{
	Cardinal data[4]; /* left, right, top, bottom */
	
	data[0] = data[1] = data[3] = LowerBorderWidth(pCD);
	data[2] = TitleBarHeight(pCD);
	XChangeProperty(DISPLAY,pCD->client,ewmh_atoms[_NET_FRAME_EXTENTS],
		XA_CARDINAL,32,PropModeReplace,(unsigned char*)data,
		sizeof(data)/sizeof(Cardinal));
}

/*
 * Called by GetClientInfo to set up initial EWMH data for new clients
 */
void ProcessEwmh(ClientData *pCD)
{
	char *sz;
	Pixmap icon;
	
	sz = FetchWindowProperty(pCD->client,
		ewmh_atoms[_NET_WM_NAME],XA_UTF8_STRING,NULL);
	if(sz){
		if(pCD->ewmhClientTitle) XmStringFree(pCD->ewmhClientTitle);
		pCD->ewmhClientTitle = XmStringCreateLocalized(sz);
		XFree(sz);
	}	


	sz = FetchWindowProperty(pCD->client,
		ewmh_atoms[_NET_WM_ICON_NAME],XA_UTF8_STRING,NULL);
	if(sz){
		if(pCD->ewmhIconTitle) XmStringFree(pCD->ewmhIconTitle);
		pCD->ewmhIconTitle = XmStringCreateLocalized(sz);
		XFree(sz);
	}

	if(pCD->ewmhIconPixmap) XFreePixmap(DISPLAY,pCD->ewmhIconPixmap);
	if((icon = GetEwmhIconPixmap(pCD))){
		pCD->ewmhIconPixmap = MakeClientIconPixmap(pCD,icon,None);
		XFreePixmap(DISPLAY,icon);
	}

	UpdateEwmhFrameExtents(pCD);
}

/*
 * Called by HandleEventsOnClientWindow to update EWMH data after
 * ECCC properties have been processed.
 */
void HandleEwmhCPropertyNotify(ClientData *pCD, XPropertyEvent *evt)
{
	if(evt->atom == ewmh_atoms[_NET_WM_NAME]) {
		char *sz = FetchWindowProperty(pCD->client,
			ewmh_atoms[_NET_WM_NAME],XA_UTF8_STRING,NULL);
		if(sz){
			if(pCD->ewmhClientTitle) XmStringFree(pCD->ewmhClientTitle);
			pCD->ewmhClientTitle = XmStringCreateLocalized(sz);
			XFree(sz);
		}	
	}
	else if(evt->atom == ewmh_atoms[_NET_WM_ICON_NAME]) {
		char *sz = FetchWindowProperty(pCD->client,
			ewmh_atoms[_NET_WM_ICON_NAME],XA_UTF8_STRING,NULL);
		if(sz){
			if(pCD->ewmhIconTitle) XmStringFree(pCD->ewmhIconTitle);
			pCD->ewmhIconTitle = XmStringCreateLocalized(sz);
			XFree(sz);
		}	
	}
	else if(evt->atom == ewmh_atoms[_NET_WM_ICON]) {
		Pixmap icon;
		if(pCD->ewmhIconPixmap) XFreePixmap(DISPLAY,pCD->ewmhIconPixmap);
		if((icon = GetEwmhIconPixmap(pCD))){
			pCD->ewmhIconPixmap = MakeClientIconPixmap(pCD,icon,None);
			XFreePixmap(DISPLAY,icon);
		}
	}
	else if(evt->atom == ewmh_atoms[_NET_REQUEST_FRAME_EXTENTS]) {
		UpdateEwmhFrameExtents(pCD);
	}
}

/*
 * Called by HandleEventsOnClientWindow.
 * Processes EWMH reladed ClientMessage events.
 */
void HandleEwmhClientMessage(ClientData *pCD, XClientMessageEvent *evt)
{
	if(evt->message_type == ewmh_atoms[_NET_WM_STATE]){
		enum { remove, add, toggle } action = evt->data.l[0];
		
		if(evt->data.l[1] == ewmh_atoms[_NET_WM_STATE_FULLSCREEN]){
			Boolean set;
			
			if(action == toggle)
				set = pCD->fullScreen?False:True;
			else
				set = (action == add)?True:False;
			
			ConfigureEwmhFullScreen(pCD,set);
		}	
	}
}

/*
 * Removes decorations and resizes frame and client windows to the size of
 * display or xinerama screen they're on if 'set' is true, restores normal
 * configuration otherwise. Sets client's _NET_WM_STATE property appropriately.
 */
void ConfigureEwmhFullScreen(ClientData *pCD, Boolean set)
{
	int xorg = 0;
	int yorg = 0;
	int swidth;
	int sheight;
	Boolean xinerama = False;
	XineramaScreenInfo xsi;
	int i;
	
	pCD->fullScreen = set;
	
	xinerama = GetXineramaScreenFromLocation(pCD->clientX,pCD->clientY,&xsi);
	if(xinerama){
		xorg = xsi.x_org;
		yorg = xsi.y_org;
		swidth = xsi.width;
		sheight = xsi.height;
	}else{
		swidth = XDisplayWidth(DISPLAY,pCD->pSD->screen);
		sheight = XDisplayHeight(DISPLAY,pCD->pSD->screen);
	}
	
	if(set){
		Atom state;
		
		XUnmapWindow(DISPLAY,pCD->clientTitleWin);
		for(i = 0; i < STRETCH_COUNT; i++){
			XUnmapWindow(DISPLAY,pCD->clientStretchWin[i]);
		}
		XMoveResizeWindow(DISPLAY,pCD->clientFrameWin,xorg,yorg,swidth,sheight);
		XMoveResizeWindow(DISPLAY,pCD->clientBaseWin,xorg,yorg,swidth,sheight);
		XResizeWindow(DISPLAY,pCD->client,swidth,sheight);
		
		state = ewmh_atoms[_NET_WM_STATE_FULLSCREEN];
		XChangeProperty(DISPLAY,pCD->client,ewmh_atoms[_NET_WM_STATE],
			XA_ATOM,32,PropModeReplace,(unsigned char*)&state,1);
	}else{
		XMoveResizeWindow(DISPLAY,pCD->clientBaseWin,
			pCD->clientOffset.x,pCD->clientOffset.y,
			pCD->clientWidth, pCD->clientHeight);
		if (pCD->maxConfig){
			XResizeWindow (DISPLAY, pCD->client,
				pCD->maxWidth, pCD->maxHeight);
		}else{
			XResizeWindow (DISPLAY, pCD->client,
				pCD->clientWidth, pCD->clientHeight);
		}
		RegenerateClientFrame(pCD);
		XMapWindow(DISPLAY,pCD->clientTitleWin);
		for(i = 0; i < STRETCH_COUNT; i++){
			XMapWindow(DISPLAY,pCD->clientStretchWin[i]);
		}
		XDeleteProperty(DISPLAY,pCD->client,ewmh_atoms[_NET_WM_STATE]);
	}
}

/*
 * Retrieves RGBA image data from the _NET_WM_ICON property. 
 * For True/DirectColor visuals, if an icon of appropriate size (larger or
 * equal to MWM icon size) is available it will be converted to a pixmap.
 * Returns a valid pixmap on success, or None.
 */
static Pixmap GetEwmhIconPixmap(const ClientData *pCD)
{
	unsigned long *prop_data;
	unsigned long *rgb_data;
	unsigned long prop_data_size;
	unsigned int rgb_width = 0;
	unsigned int rgb_height = 0;
	XImage *dest_img;
	Visual *visual;
	GC gc;
	XGCValues gcv;
	int depth;
	XColor bg = {0};
	Pixmap pixmap = None;
	unsigned short red_shift = 0;
	unsigned short green_shift = 0;
	unsigned short blue_shift = 0;
	union {	unsigned long *i; unsigned char *b; } fptr;
	unsigned int x, y;
	float dx, dy;
	/* icon size we actually want, sans space for decorations
	 * MakeClientIconPixmap adds later on */
	unsigned int icon_width =
		PSD_FOR_CLIENT(pCD)->iconImageMaximum.width - 
		 (2 * ICON_INTERNAL_SHADOW_WIDTH +
		 ((wmGD.frameStyle == WmSLAB) ? 2 : 0));
    unsigned int icon_height =
		PSD_FOR_CLIENT(pCD)->iconImageMaximum.height -
		 ( 2 * ICON_INTERNAL_SHADOW_WIDTH +
		  ((wmGD.frameStyle == WmSLAB) ? 2 : 0));
	
	
	visual = XDefaultVisual(DISPLAY,SCREEN_FOR_CLIENT(pCD));
	depth = XDefaultDepth(DISPLAY,SCREEN_FOR_CLIENT(pCD));
	if(visual->class != TrueColor && visual->class != DirectColor)
		return None;
	
	prop_data = FetchWindowProperty(pCD->client,
		ewmh_atoms[_NET_WM_ICON],XA_CARDINAL,&prop_data_size);
	if(!prop_data) return None;
	
	/* loop trough available images looking for usable size */
	fptr.i = prop_data;
	rgb_width = fptr.i[0];
	rgb_height = fptr.i[1];

	while(rgb_width < icon_width || rgb_height < icon_height) {
		if(!rgb_width || !rgb_height){
			XFree(prop_data);
			return None;
		}
		
		if((fptr.i + 2+rgb_width*rgb_height) >= 
			(prop_data + prop_data_size)) {
				XFree(prop_data);
				return None;
		}
		
		fptr.i += 2+rgb_width*rgb_height;
		rgb_width = fptr.i[0];
		rgb_height = fptr.i[1];
	};
	rgb_data = fptr.i;
	
	dest_img = XCreateImage(DISPLAY,visual,depth,ZPixmap,0,NULL,
		icon_width,icon_height, XBitmapPad(DISPLAY),0);
	if(dest_img){
		dest_img->data = malloc(
			(dest_img->width*dest_img->height) * (dest_img->bitmap_pad/8));
		if(!dest_img->data){
			XDestroyImage(dest_img);
			XFree(prop_data);
			return None;
		}
	}
	
	/* color we want to aplha-blend the image with */
	bg.pixel = ICON_APPEARANCE(pCD).background;
	XQueryColor(DISPLAY,pCD->clientColormap,&bg);
	bg.red >>= 8; bg.green >>= 8; bg.blue >>= 8;
	
	/* figure out what server pixels are like */
	red_shift = visual->red_mask?ffs(visual->red_mask)-1:0;
	green_shift = visual->green_mask?ffs(visual->green_mask)-1:0;
	blue_shift = visual->blue_mask?ffs(visual->blue_mask)-1:0;
	
	/* scale and alpha-blend RGBA to XImage, then  make a pixmap out of it */
	dx = (float)rgb_width / dest_img->width;
	dy = (float)rgb_height / dest_img->height;
	
	for(y = 0; y < dest_img->height; y++){
		for(x = 0; x < dest_img->width; x++){
			unsigned int r,g,b;
			float a;
			unsigned long pixel;
			
			fptr.i = rgb_data + 
				((unsigned int)(y * dy) * rgb_width + (unsigned int)(x * dx));
			a = (float)fptr.b[3] / 256;
			r = ((float)fptr.b[2] * a + (float)bg.red * (1.0 - a));
			g = ((float)fptr.b[1] * a + (float)bg.green * (1.0 - a));
			b = ((float)fptr.b[0] * a + (float)bg.blue * (1.0 - a));
			
			pixel = (r << red_shift) | (g << green_shift) | (b << blue_shift);
			
			XPutPixel(dest_img,x,y,pixel);
		}
	}

	pixmap = XCreatePixmap(DISPLAY,pCD->client,
		icon_width,icon_height,depth);	
	if(!pixmap){
		XDestroyImage(dest_img);
		XFree(prop_data);
		return None;
	}
	
	gcv.graphics_exposures = False;
	gc = XCreateGC(DISPLAY,pCD->client,GCGraphicsExposures,&gcv);
	if(gc){
		XPutImage(DISPLAY,pixmap,gc,dest_img,0,0,0,0,
			dest_img->width,dest_img->height);
		XFreeGC(DISPLAY,gc);
	}
	
	XDestroyImage(dest_img);
	XFree(prop_data);
	
	return pixmap;
}

/*
 * Retrieves property data of specified type from the given window.
 * Returns property data and sets size to the number of items of
 * requested type on success. Returns NULL if no data available or
 * is not of type specified.
 * The caller is responsible for freeing the memory.
 */
static void* FetchWindowProperty(Window wnd, Atom prop,
	Atom req_type, unsigned long *size)
{
	unsigned long ret_items, ret_bytes_left;
	char *result = NULL;
	Atom ret_type;
	int ret_fmt;
	
	XGetWindowProperty(DISPLAY,wnd,prop,0,BUFSIZ,
		False,req_type,&ret_type,&ret_fmt,&ret_items,
		&ret_bytes_left,(unsigned char**)&result);
	
	if(ret_type!=req_type){
		if(result) XFree(result);
		return NULL;
	}
	
	if(ret_bytes_left){
		XFree(result);
		XGetWindowProperty(DISPLAY,wnd,prop,0,BUFSIZ+ret_bytes_left+1,
			False,req_type,&ret_type,&ret_fmt,&ret_items,
			&ret_bytes_left,(unsigned char**)&result);

	}

	if(size) *size = ret_items;
	return result;
}