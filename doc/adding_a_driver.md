This is intended to be a quick guide / reference on how you can write
your own i/o drivers for dnscat2! To give you an idea of what they can
do, here are some examples of i/o drivers:

- Console (turned on with --console, lets users type messages to the
server)
- Exec (turned on with --exec, tunnels i/o to/from a process)
- Command (default; an interactive session that can spawn other
sessions)

This guide is about how to add others!

Here are the steps:
- Update drivers/driver.(c|h) to add simple wrappers to handle the
driver type (discount polymorphism)
- Create a .c and .h file that implement the following methods:
 - driver_XXX_t \*driver_XXX_create(select_group_t \*group);
 - void driver_XXX_destroy(driver_XXX_t \*driver);
 - void driver_XXX_data_received(driver_XXX_t \*driver, uint8_t \*data, size_t length);
 - uint8_t \*driver_XXX_get_outgoing(driver_XXX_t \*driver, size_t \*length, size_t max_length);
 - void driver_XXX_close(driver_XXX_t \*driver);
- Add the .o for the driver to the Makefile under OBJS
- Add the following function to controller/session.(c|h):
  - session_t \*session_create_XXX(select_group_t \*group, char \*name)
  - If it's a sub-protocol - that is, anything that the server needs to handle in a special way - be sure to set some SYN options that the server will see
- Make it possible to create the driver
  - Update main() in dnscat.c with commandline options to create the driver (don't forget to update 'usage'!)
  - Update driver_command.c with the ability to create instances of it
