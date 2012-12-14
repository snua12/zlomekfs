/*
    Copyright (c) 2006-2011, Alexis Royer, http://alexis.royer.free.fr/CLI

    All rights reserved.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
        * Neither the name of the CLI library project nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
    CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package cli;


/** Input/output device multiplexer.
    Manages a list of input devices, and output streams can be specialized. */
public class IOMux extends IODevice.Native
{
    /** Default constructor. */
    public IOMux() {
        super(__IOMux());
    }
    private static final native int __IOMux();

    /** Destructor. */
    protected void finalize() throws Throwable {
        if (getbDoFinalize()) {
            __finalize(this.getNativeRef());
            dontFinalize(); // finalize once.
        }
        super.finalize();
    }
    private static final native void __finalize(int I_NativeIOMuxRef);

    /** Device addition in the list.
        @param CLI_Device Input / output device.
        @return true if the device has been added, false otherwise. */
    public boolean addDevice(IODevice.Interface CLI_Device) {
        return __addDevice(this.getNativeRef(), CLI_Device.getNativeRef());
    }
    private static final native boolean __addDevice(int I_NativeIOMuxRef, int I_NativeDeviceRef);

    /** Current device accessor.
        @return Current device. */
    public IODevice.Interface getCurrentDevice() {
        NativeObject cli_Device = NativeObject.getObject(__getCurrentDevice(this.getNativeRef()));
        if (cli_Device instanceof IODevice.Interface) {
            return (IODevice.Interface) cli_Device;
        } else {
            return null;
        }
    }
    private static final native int __getCurrentDevice(int I_NativeIOMuxRef);

    /** Switch to next device.
        @return Next device if success, null otherwise. */
    public IODevice.Interface switchNextDevice() {
        NativeObject cli_Device = NativeObject.getObject(__switchNextDevice(this.getNativeRef()));
        if (cli_Device instanceof IODevice.Interface) {
            return (IODevice.Interface) cli_Device;
        } else {
            return null;
        }
    }
    private static final native int __switchNextDevice(int I_NativeIOMuxRef);

    //! @brief Reset device list.
    //! @return true for success, false otherwise.
    boolean resetDeviceList() {
        return __resetDeviceList(this.getNativeRef());
    }
    private static final native boolean __resetDeviceList(int I_NativeIOMuxRef);
}
