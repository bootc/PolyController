// Martin Thomas 4/2008

#ifndef PORT_EXT_H
#define PORT_EXT_H

void port_ext_init(void);
void port_ext_update(void);
void port_ext_bit_clear( uint8_t port, uint8_t bit );
void port_ext_bit_set( uint8_t port, uint8_t bit );
void port_ext_set( uint8_t port, uint8_t val );

#endif
