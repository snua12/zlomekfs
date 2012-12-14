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

#include "cli/pch.h"

#include <iostream>

#include "cli/tk.h"

#include "cli/traces.h"
#include "cli/io_device.h"

CLI_NS_USE(cli)


const bool CheckString(void)
{
    // New string.
    tk::String tk_String(10);
    if (! tk_String.IsEmpty()) {
        std::cerr << "tk::String: new String is not empty." << std::endl;
        return false;
    }
    if (tk_String.GetLength() != 0) {
        std::cerr << "tk::String: new String length is not 0." << std::endl;
        return false;
    }
    if ((const char*) tk_String == NULL) {
        std::cerr << "tk::String: implicit conversion does not work on a new String." << std::endl;
        return false;
    }
    if (strcmp(tk_String, "") != 0) {
        std::cerr << "tk::String: new String does not equal ''." << std::endl;
        return false;
    }

    // 0 character string.
    if (! tk_String.Set("")) {
        std::cerr << "tk:String: could not set ''." << std::endl;
        return false;
    }
    if (! tk_String.IsEmpty()) {
        std::cerr << "tk::String: string is not empty after setting ''." << std::endl;
        return false;
    }
    if (tk_String.GetLength() != 0) {
        std::cerr << "tk::String: length is not 0 after setting ''." << std::endl;
        return false;
    }
    if (strcmp(tk_String, "") != 0) {
        std::cerr << "tk:String: tk_String differs from ''." << std::endl;
        return false;
    }

    // 3 characters string.
    if (! tk_String.Set("abc")) {
        std::cerr << "tk:String: could not set 'abc'." << std::endl;
        return false;
    }
    if (tk_String.IsEmpty()) {
        std::cerr << "tk::String: string is empty after setting 'abc'." << std::endl;
        return false;
    }
    if (tk_String.GetLength() != 3) {
        std::cerr << "tk::String: length is not 3 after setting 'abc'." << std::endl;
        return false;
    }
    if (strcmp(tk_String, "abc") != 0) {
        std::cerr << "tk:String: tk_String differs from 'abc'." << std::endl;
        return false;
    }

    // 10 characters string.
    if (! tk_String.Set("abcdefghij")) {
        std::cerr << "tk:String: could not set 'abcdefghij'." << std::endl;
        return false;
    }
    if (tk_String.IsEmpty()) {
        std::cerr << "tk::String: string is empty after setting 'abcdefghij'." << std::endl;
        return false;
    }
    if (tk_String.GetLength() != 10) {
        std::cerr << "tk::String: length is not 10 after setting 'abcdefghij'." << std::endl;
        return false;
    }
    if (strcmp(tk_String, "abcdefghij") != 0) {
        std::cerr << "tk:String: tk_String differs from 'abcdefghij'." << std::endl;
        return false;
    }

    // 11 characters string.
    if (tk_String.Set("abcdefghijk")) {
        std::cerr << "tk:String: 'abcdefghijk' set without trouble." << std::endl;
        return false;
    }
    if (tk_String.IsEmpty()) {
        std::cerr << "tk::String: string is empty after setting 'abcdefghijk'." << std::endl;
        return false;
    }
    if (tk_String.GetLength() != 10) {
        std::cerr << "tk::String: length is not 10 after setting 'abcdefghijk'." << std::endl;
        return false;
    }
    if (strcmp(tk_String, "abcdefghij") != 0) {
        std::cerr << "tk:String: tk_String differs from 'abcdefghij'." << std::endl;
        return false;
    }

    // Sub-string.
    if (strcmp(tk_String.SubString(1, 3), "bcd") != 0) {
        std::cerr << "tk::String: SubString(1, 3) failure." << std::endl;
        return false;
    }
    if (strcmp(tk_String.SubString(0, 0), "") != 0) {
        std::cerr << "tk::String: SubString(0, 0) failure." << std::endl;
        return false;
    }

    // Upper / Lower operations.
    if (strcmp(tk_String.ToUpper(), "ABCDEFGHIJ") != 0) {
        std::cerr << "tk::String: ToUpper() failure." << std::endl;
    }
    if (strcmp(tk_String.ToUpper().ToLower(), "abcdefghij") != 0) {
        std::cerr << "tk::String: ToLower() failure." << std::endl;
    }

    // Equal operator.
    bool b_Res = (tk_String == "abcdefghij");
    if (! b_Res) {
        std::cerr << "tk::String: equal operator failure." << std::endl;
        return false;
    }
    b_Res = (tk_String == "abcdefghijk");
    if (b_Res) {
        std::cerr << "tk::String: equal operator failure." << std::endl;
        return false;
    }

    // Difference operator.
    b_Res = (tk_String != "abcdefghij");
    if (b_Res) {
        std::cerr << "tk::String: difference operator failure." << std::endl;
        return false;
    }
    b_Res = (tk_String != "abcdefghijk");
    if (! b_Res) {
        std::cerr << "tk::String: difference operator failure." << std::endl;
        return false;
    }

    return true;
}

const bool CheckQueues(void)
{
    // New queue.
    tk::Queue<int> tk_Queue(3);
    if (! tk_Queue.IsEmpty()) {
        std::cerr << "tk::Queue: new queue is not empty." << std::endl;
        return false;
    }
    if (tk_Queue.GetCount() != 0) {
        std::cerr << "tk::Queue: new queue element count is not 0." << std::endl;
        return false;
    }

    // First element addition.
    if (! tk_Queue.AddTail(1)) {
        std::cerr << "tk::Queue: first element addition failure." << std::endl;
        return false;
    }
    if (tk_Queue.IsEmpty()) {
        std::cerr << "tk::Queue: queue is empty after first element addition." << std::endl;
        return false;
    }
    if (tk_Queue.GetCount() != 1) {
        std::cerr << "tk::Queue: queue element count is not 1 after first element addition." << std::endl;
        return false;
    }
    if ((tk_Queue.GetHead() != 1) || (tk_Queue.GetTail() != 1)) {
        std::cerr << "tk::Queue: head is not 1 or tail is not 1 after first element addition." << std::endl;
        return false;
    }

    // Second element addition.
    if (! tk_Queue.AddTail(2)) {
        std::cerr << "tk::Queue: second element addition failure." << std::endl;
        return false;
    }
    if (tk_Queue.IsEmpty()) {
        std::cerr << "tk::Queue: queue is empty after second element addition." << std::endl;
        return false;
    }
    if (tk_Queue.GetCount() != 2) {
        std::cerr << "tk::Queue: queue element count is not 2 after second element addition." << std::endl;
        return false;
    }
    if ((tk_Queue.GetHead() != 1) || (tk_Queue.GetTail() != 2)) {
        std::cerr << "tk::Queue: head is not 1 or tail is not 2 after second element addition." << std::endl;
        return false;
    }

    // Third element addition.
    if (! tk_Queue.AddHead(0)) {
        std::cerr << "tk::Queue: third element addition failure." << std::endl;
        return false;
    }
    if (tk_Queue.IsEmpty()) {
        std::cerr << "tk::Queue: queue is empty after third element addition." << std::endl;
        return false;
    }
    if (tk_Queue.GetCount() != 3) {
        std::cerr << "tk::Queue: queue element count is not 3 after third element addition." << std::endl;
        return false;
    }
    if ((tk_Queue.GetHead() != 0) || (tk_Queue.GetTail() != 2)) {
        std::cerr << "tk::Queue: head is not 0 or tail is not 2 after third element addition." << std::endl;
        return false;
    }

    // Fourth element addition.
    if (tk_Queue.AddTail(3)) {
        std::cerr << "tk::Queue: fourth element addition succeeded." << std::endl;
        return false;
    }
    if (tk_Queue.IsEmpty()) {
        std::cerr << "tk::Queue: queue is empty after fourth element addition." << std::endl;
        return false;
    }
    if (tk_Queue.GetCount() != 3) {
        std::cerr << "tk::Queue: queue element count is not 3 after fourth element addition." << std::endl;
        return false;
    }
    if ((tk_Queue.GetHead() != 0) || (tk_Queue.GetTail() != 2)) {
        std::cerr << "tk::Queue: head is not 0 or tail is not 2 after fourth element addition." << std::endl;
        return false;
    }

    // Fifth element addition.
    if (tk_Queue.AddHead(-1)) {
        std::cerr << "tk::Queue: fifth element addition succeeded." << std::endl;
        return false;
    }
    if (tk_Queue.IsEmpty()) {
        std::cerr << "tk::Queue: queue is empty after fifth element addition." << std::endl;
        return false;
    }
    if (tk_Queue.GetCount() != 3) {
        std::cerr << "tk::Queue: queue element count is not 3 after fifth element addition." << std::endl;
        return false;
    }
    if ((tk_Queue.GetHead() != 0) || (tk_Queue.GetTail() != 2)) {
        std::cerr << "tk::Queue: head is not 0 or tail is not 2 after fifth element addition." << std::endl;
        return false;
    }

    // Iteration.
    do {
        int i_Item = 0;
        for (   tk::Queue<int>::Iterator it = tk_Queue.GetIterator();
                tk_Queue.IsValid(it);
                tk_Queue.MoveNext(it))
        {
            if (tk_Queue.GetAt(it) != i_Item)
            {
                std::cerr << "tk::Queue: element mismatch on iteration." << std::endl;
                return false;
            }
            else
            {
                i_Item ++;
            }
        }
    } while(0);

    // Sort.
    class _ { public:
        static const int cmp(const int& i_1, const int& i_2) {
            if (0) {}
            else if (i_1 < i_2) { return -1; }
            else if (i_1 > i_2) { return 1; }
            else { return 0; }
        }
    };
    if (! tk_Queue.Sort(_::cmp)) {
        std::cerr << "tk::String: sort failure." << std::endl;
        return false;
    }
    do {
        int i_Item = 2;
        for (   tk::Queue<int>::Iterator it = tk_Queue.GetIterator();
                tk_Queue.IsValid(it);
                tk_Queue.MoveNext(it))
        {
            if (tk_Queue.GetAt(it) != i_Item)
            {
                std::cerr << "tk::Queue: element mismatch after sorting." << std::endl;
                return false;
            }
            else
            {
                i_Item --;
            }
        }
    } while(0);

    // First element removal.
    if (tk_Queue.RemoveTail() != 0) {
        std::cerr << "tk::Queue: first element removal failure." << std::endl;
        return false;
    }
    if (tk_Queue.IsEmpty()) {
        std::cerr << "tk::Queue: queue is empty after first element removal." << std::endl;
        return false;
    }
    if (tk_Queue.GetCount() != 2) {
        std::cerr << "tk::Queue: queue element count is not 2 after first element removal." << std::endl;
        return false;
    }
    if ((tk_Queue.GetHead() != 2) || (tk_Queue.GetTail() != 1)) {
        std::cerr << "tk::Queue: head is not 2 or tail is not 1 after first element removal." << std::endl;
        return false;
    }

    // Second element removal.
    if (tk_Queue.RemoveHead() != 2) {
        std::cerr << "tk::Queue: second element removal failure." << std::endl;
        return false;
    }
    if (tk_Queue.IsEmpty()) {
        std::cerr << "tk::Queue: queue is empty after second element removal." << std::endl;
        return false;
    }
    if (tk_Queue.GetCount() != 1) {
        std::cerr << "tk::Queue: queue element count is not 1 after second element removal." << std::endl;
        return false;
    }
    if ((tk_Queue.GetHead() != 1) || (tk_Queue.GetTail() != 1)) {
        std::cerr << "tk::Queue: head is not 1 or tail is not 1 after second element removal." << std::endl;
        return false;
    }

    // Third element removal.
    if (tk_Queue.RemoveTail() != 1) {
        std::cerr << "tk::Queue: third element removal failure." << std::endl;
        return false;
    }
    if (! tk_Queue.IsEmpty()) {
        std::cerr << "tk::Queue: queue is not empty after third element removal." << std::endl;
        return false;
    }
    if (tk_Queue.GetCount() != 0) {
        std::cerr << "tk::Queue: queue element count is not 0 after third element removal." << std::endl;
        return false;
    }

    return true;
}

const bool CheckMaps(void)
{
    // New map.
    tk::Map<tk::String, int> tk_Map(3);
    if (! tk_Map.IsEmpty()) {
        std::cerr << "tk::Map: new map is not empty." << std::endl;
        return false;
    }
    if (tk_Map.GetCount() != 0) {
        std::cerr << "tk::Map: new map element count is not 0." << std::endl;
        return false;
    }

    // First element setting.
    if (! tk_Map.SetAt(tk::String(5, "1"), 1)) {
        std::cerr << "tk::Map: SetAt(1) failure" << std::endl;
        return false;
    }
    if (tk_Map.IsEmpty()) {
        std::cerr << "tk::Map: tk_Map is empty after SetAt(1)" << std::endl;
        return false;
    }
    if (tk_Map.GetCount() != 1) {
        std::cerr << "tk::Map: tk_Map count is not 1 after SetAt(1)" << std::endl;
        return false;
    }
    if (! tk_Map.IsSet(tk::String(3, "1"))) {
        std::cerr << "tk::Map: '1' key is not set after SetAt(1)" << std::endl;
        return false;
    }
    if (tk_Map.IsSet(tk::String(3, "2"))) {
        std::cerr << "tk::Map: '2' key is set after SetAt(2)" << std::endl;
        return false;
    }

    // Second element setting.
    if (! tk_Map.SetAt(tk::String(5, "2"), 2)) {
        std::cerr << "tk::Map: SetAt(2) failure" << std::endl;
        return false;
    }
    if (tk_Map.IsEmpty()) {
        std::cerr << "tk::Map: tk_Map is empty after SetAt(2)" << std::endl;
        return false;
    }
    if (tk_Map.GetCount() != 2) {
        std::cerr << "tk::Map: tk_Map count is not 2 after SetAt(2)" << std::endl;
        return false;
    }
    if (! tk_Map.IsSet(tk::String(3, "1"))) {
        std::cerr << "tk::Map: '1' key is not set after SetAt(2)" << std::endl;
        return false;
    }
    if (! tk_Map.IsSet(tk::String(3, "2"))) {
        std::cerr << "tk::Map: '2' key is set after SetAt(2)" << std::endl;
        return false;
    }

    // Iteration.
    tk::Queue<tk::String> tk_Keys(10);
    tk::Queue<int> tk_Values(10);
    for (   tk::Map<tk::String, int>::Iterator it = tk_Map.GetIterator();
            tk_Map.IsValid(it);
            tk_Map.MoveNext(it))
    {
        tk_Keys.AddTail(tk_Map.GetKey(it));
        tk_Values.AddTail(tk_Map.GetAt(it));
    }
    class _ { public:
        static const int CmpInt(const int& I1, const int& I2) {
            if (0) {}
            else if (I1 < I2) return 1;
            else if (I1 > I2) return -1;
            else return 0;
        }
        static const int CmpString(const tk::String& STR1, const tk::String& STR2) {
            if (0) {}
            else if (STR1 < STR2) return 1;
            else if (STR2 > STR1) return -1;
            else return 0;
        }
    };
    tk_Keys.Sort(_::CmpString);
    tk_Values.Sort(_::CmpInt);

    // GetAt.
    if (const int* const pi_1 = tk_Map.GetAt(tk::String(3, "1"))) {
        if (*pi_1 != 1) {
            std::cerr << "tk::Map: GetAt(1) does not match the element 1." << std::endl;
            return false;
        }
    } else {
        std::cerr << "tk::Map: GetAt(1) failure." << std::endl;
        return false;
    }
    if (const int* const pi_2 = tk_Map.GetAt(tk::String(3, "2"))) {
        if (*pi_2 != 2) {
            std::cerr << "tk::Map: GetAt(2) does not match the element 2." << std::endl;
            return false;
        }
    } else {
        std::cerr << "tk::Map: GetAt(2) failure." << std::endl;
        return false;
    }

    // First element removal.
    if (! tk_Map.Unset(tk::String(3, "1"))) {
        std::cerr << "tk::Map: Unset(1) failure." << std::endl;
        return false;
    }
    if (! tk_Map.Unset(tk::String(3, "1"))) {
        std::cerr << "tk::Map: second call to Unset(1) failure." << std::endl;
        return false;
    }
    if (tk_Map.IsEmpty()) {
        std::cerr << "tk::Map: map is empty after Unset(1)." << std::endl;
        return false;
    }
    if (tk_Map.GetCount() != 1) {
        std::cerr << "tk::Map: map element count is not 1 after Unset(1)." << std::endl;
        return false;
    }
    if (tk_Map.IsSet(tk::String(3, "1"))) {
        std::cerr << "tk::Map: '1' is still set after Unset(1)." << std::endl;
        return false;
    }
    if (! tk_Map.IsSet(tk::String(3, "2"))) {
        std::cerr << "tk::Map: '2' is not set anymore after Unset(1)." << std::endl;
        return false;
    }

    // Second element removal.
    tk::Map<tk::String, int>::Iterator head = tk_Map.GetIterator();
    if (! tk_Map.IsValid(head)) {
        std::cerr << "tk::Map: head iterator is not valid while there are 2 elements in the map." << std::endl;
        return false;
    }
    if (! tk_Map.Remove(head)) {
        std::cerr << "tk::Map: Remove(head) failure." << std::endl;
        return false;
    }
    if (! tk_Map.IsEmpty()) {
        std::cerr << "tk::Map: map is not empty after Remove(head)." << std::endl;
        return false;
    }
    if (tk_Map.GetCount() != 0) {
        std::cerr << "tk::Map: map element count is not 0 after Remove(head)." << std::endl;
        return false;
    }
    if (tk_Map.IsSet(tk::String(3, "1"))) {
        std::cerr << "tk::Map: '1' is still set after Remove(head)." << std::endl;
        return false;
    }
    if (tk_Map.IsSet(tk::String(3, "2"))) {
        std::cerr << "tk::Map: '2' is still set anymore after Remove(head)." << std::endl;
        return false;
    }

    return true;
}


int main(void)
{
    if (! CheckString())
        return -1;
    if (! CheckQueues())
        return -1;
    if (! CheckMaps())
        return -1;

    return 0;
}
