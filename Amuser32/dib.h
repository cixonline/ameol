WORD FASTCALL PaletteSize (VOID FAR * pv);
WORD FASTCALL DibNumColors (VOID FAR * pv);
HPALETTE FASTCALL CreateDibPalette (HANDLE hdib);
HPALETTE FASTCALL CreateBIPalette (LPBITMAPINFOHEADER lpbi);
HANDLE FASTCALL BitmapFromDib (HANDLE hdib, HPALETTE hpal);
BOOL FASTCALL DibInfo (HANDLE hdib,LPBITMAPINFOHEADER lpbi);
HANDLE FASTCALL ReadDibBitmapInfo (int fh);
HBITMAP DIBToBitmap(HANDLE, HPALETTE );