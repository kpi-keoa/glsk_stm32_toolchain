#################################
GlobalLogic Starter Kit resources
#################################

Requirements
************

.. note::
   Arch Linux means access to the most recent software versions. Package names and installation
   commands provided here are given for the Arch Linux and its derivatives (i.e. Manjaro).
   
   If you are using another distro, you need to figure the package names yourself or use something
   like `Archlinux Docker image <https://hub.docker.com/_/archlinux>`_.

- `OpenOCD <http://openocd.org>`_.
  
  | Stable version: ``openocd``
    (Not recommended as it is outdated and incompatible with our openocd config.
     If you prefer stable version, use ``openocd -f board/stm32f4discovery.cfg``
     instead of our ``openocd -f openocd_glstarterkit.cfg``)
  | Latest Git version: ``openocd-git`` (through AUR)
- `arm-none-eabi Toolchain <https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm>`_
  
  Official release verison:
     
     | `GCC <https://gcc.gnu.org/>`_: ``arm-none-eabi-gcc``
     | `Binutils <https://www.gnu.org/software/binutils/>`_: ``arm-none-eabi-binutils``
     | `GDB <https://www.gnu.org/software/gdb/>`_: ``arm-none-eabi-gdb``
     | `Newlib <https://sourceware.org/newlib/>`_: ``arm-none-eabi-newlib``
- `Doxygen <https://doxygen.nl>`_ and `GraphViz <https://graphviz.org/>`_ for building libopencm3 documentation
  
  ``doxygen`` and ``graphviz`` packages

Add user to plugdev group:
~~~~~~~~~~~~~~~~~~~~~~~~~
This step is required to allow working with debuggers OpenOCD supports as a user, without a need
for having root privileges.

OpenOCD package on Arch comes with udev rules file (``/usr/lib/udev/rules.d/60-openocd.rules``).
It gives access rights to users in plugdev group, which exists on Debian, but is not present
on Arch Linux. So we need to create the group and add our user to it:

.. code-block:: shell-session
   
   sudo groupadd -r -g 46 plugdev
   sudo useradd -G plugdev $USER

And log out (or reboot)

Install all packages:
~~~~~~~~~~~~~~~~~~~~~
.. code-block:: shell-session
   
   yay -S openocd-git
   sudo pacman -S arm-none-eabi-{gcc,binutils,gdb,newlib} doxygen graphviz

.. note::
   You need to either run ``sudo udevadm control --reload-rules`` and ``sudo udevadm trigger``
   or to reboot after installing OpenOCD for udev rules to start working

Documentation
*************

.. list-table:: Documents listed `here <documentation/>`_:
   :align: left
   :widths: 20 80
   :header-rows: 0
   
   * - `STM32F407 Reference Manual <documentation/STM32F407_Reference_Manual_(RM0090).pdf>`_
     - Main document on the whole STM32F4 family and its peripherals.
   * - `STM32 Cortex-M4 MCUs and MPUs Programming Manual <documentation/STM32_Cortex-M4_Programming_Manual_(PM0214).pdf>`_
     - Describes features specific to Cortex-M4 core and core-related peripherals
       (Interrupt Controller, Floating-Point Unit, ...).
   * - `STM32F407 Datasheet <documentation/STM32F407_Datasheet_(DS8626).pdf>`_
     - Datasheet describes specific features of concrete STM32F4 MCU
       (memory size, pinout, available peripherals, ...).
   * - `STM32F40x Errata <documentation/STM32F40x_Errata_(ES0182).pdf>`_
     - Chips usually have errors due to design mistakes. Errors are described and Erratas and are
       being fixed in later silicon revisions. 
       If something should work, but it does not – check errata as you may be dealing with an old
       chip revision.
   * - `STM32F4DISCOVERY User Manual <documentation/STM32F4DISCOVERY_User_Manual_(UM1472).pdf>`_
     - Document provides information on STM32F4DISCOVERY board and serves as a good quick
       reference on how to use one or another board peripheral, which is faster and easier than
       directly looking into board schematic.
   * - `STM32F4DISCOVERY Schematic <documentation/STM32F4DISCOVERY_Schematic.pdf>`_
     - Sometimes you will need to look in the schematic, in cases when the User Manual
       does not provide enough information.
   * - `GlobalLogic Starter Kit Schematic <documentation/GL-StarterKit_Schematic_rev1.1.pdf>`_
     - GL-SK kit consists of two boards. This document is the main source of information on
       external modules the GL-SK board has.


Useful links
************

- `libopencm3 Developer Documentation
  <http://libopencm3.org/docs/latest/stm32f4/html/modules.html>`_
  or build it yourself (``make TARGETS=stm32/f4 lib html`` inside libopencm3 folder)
- `STM32F407VG STMicroelectronics Resources page
  <https://www.st.com/en/microcontrollers-microprocessors/stm32f407vg.html#resource>`_
- `STM32F4DISCOVERY STMicroelectronics Resources page
  <https://www.st.com/content/st_com/en/products/evaluation-tools/product-evaluation-tools/mcu-mpu-eval-tools/stm32-mcu-mpu-eval-tools/stm32-discovery-kits/stm32f4discovery.html#resource>`_


How to start
************
#. Make sure you have completed the installation steps described in Requirements_ first.
#. Recursively clone the repository:
   
   .. code-block:: shell-session
      
      git clone --recursive https://github.com/kpi-keoa/glsk_stm32_toolchain
   
   or clone first and then initialize all submodules
   
   .. code-block:: shell-session
      
      git clone https://github.com/kpi-keoa/glsk_stm32_toolchain
      git submodule update --init --recursive
#. Study the `<Makefile>`_. It is crucial to understand how tools work for properly using them.
#. Build example projects
#. Start your own project using this repository as a template.
   
   For that, you will probably need to delete the `<documentation>`_ and example directories.
   And at least change ``TARGET`` to the name of your project top-level file

License
*******
| Everything in this repository, except the STMicroelectronics documentation is licensed
  under the MIT License.
| See `<LICENSE>`_ for details.
| 
| For more on STMicroelectronics documentation licensing consider their official website
  (`<https://st.com>`_)

Contact information
*******************
Should you have questions, feel free to contact me via Telegram
(`@thodnev <https://t.me/thodnev>`_) or e-mail (thodnev <at> xinity.dev)
