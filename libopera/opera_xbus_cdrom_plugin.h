#ifndef LIBOPERA_XBUS_CDROM_PLUGIN_H_INCLUDED
#define LIBOPERA_XBUS_CDROM_PLUGIN_H_INCLUDED

void* xbus_cdrom_plugin(int proc_, void* data_);
int   xbus_cdrom_media_ejected(void);
void  xbus_cdrom_media_eject(void);

#endif /* LIBOPERA_XBUS_CDROM_PLUGIN_H_INCLUDED */
