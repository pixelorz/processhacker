/*
 * Process Hacker - 
 *   GUI support functions
 * 
 * Copyright (C) 2009-2010 wj32
 * 
 * This file is part of Process Hacker.
 * 
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#define GUISUP_PRIVATE
#include <phgui.h>

_ChangeWindowMessageFilter ChangeWindowMessageFilter_I;
_SetWindowTheme SetWindowTheme_I;
_TaskDialogIndirect TaskDialogIndirect_I;

VOID PhGuiSupportInitialization()
{
    LoadLibrary(L"uxtheme.dll");

    if (WINDOWS_HAS_UAC)
        ChangeWindowMessageFilter_I = PhGetProcAddress(L"user32.dll", "ChangeWindowMessageFilter");
    SetWindowTheme_I = PhGetProcAddress(L"uxtheme.dll", "SetWindowTheme");
    TaskDialogIndirect_I = PhGetProcAddress(L"comctl32.dll", "TaskDialogIndirect");
}

VOID PhSetControlTheme(
    __in HWND Handle,
    __in PWSTR Theme
    )
{
    if (WindowsVersion >= WINDOWS_VISTA)
    {
        if (SetWindowTheme_I)
            SetWindowTheme_I(Handle, Theme, NULL);
    }
}

HWND PhCreateListViewControl(
    __in HWND ParentHandle,
    __in INT_PTR Id
    )
{
    return CreateWindow(
        WC_LISTVIEW,
        L"",
        WS_CHILD | LVS_REPORT | WS_VISIBLE | WS_BORDER,
        0,
        0,
        3,
        3,
        ParentHandle,
        (HMENU)Id,
        PhInstanceHandle,
        NULL
        );
}

INT PhAddListViewColumn(
    __in HWND ListViewHandle,
    __in INT Index,
    __in INT DisplayIndex,
    __in INT SubItemIndex,
    __in INT Format,
    __in INT Width,
    __in PWSTR Text
    )
{
    LVCOLUMN column;

    column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_ORDER;
    column.fmt = Format;
    column.cx = Width;
    column.pszText = Text;
    column.iSubItem = SubItemIndex;
    column.iOrder = DisplayIndex;

    return ListView_InsertColumn(ListViewHandle, Index, &column);
}

INT PhAddListViewItem(
    __in HWND ListViewHandle,
    __in INT Index,
    __in PWSTR Text,
    __in PVOID Param
    )
{
    LVITEM item;

    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = Index;
    item.iSubItem = 0;
    item.pszText = Text;
    item.lParam = (LPARAM)Param;

    return ListView_InsertItem(ListViewHandle, &item);
}

INT PhFindListViewItemByFlags(
    __in HWND ListViewHandle,
    __in INT StartIndex,
    __in ULONG Flags
    )
{
    return ListView_GetNextItem(ListViewHandle, StartIndex, Flags);
}

INT PhFindListViewItemByParam(
    __in HWND ListViewHandle,
    __in INT StartIndex,
    __in PVOID Param
    )
{
    LVFINDINFO findInfo;

    findInfo.flags = LVFI_PARAM;
    findInfo.lParam = (LPARAM)Param;

    return ListView_FindItem(ListViewHandle, StartIndex, &findInfo);
}

LOGICAL PhGetListViewItemParam(
    __in HWND ListViewHandle,
    __in INT Index,
    __out PPVOID Param
    )
{
    LOGICAL result;
    LVITEM item;

    item.mask = LVIF_PARAM;
    item.iItem = Index;

    result = ListView_GetItem(ListViewHandle, &item);

    if (!result)
        return result;

    *Param = (PVOID)item.lParam;

    return result;
}

VOID PhRemoveListViewItem(
    __in HWND ListViewHandle,
    __in INT Index
    )
{
    ListView_DeleteItem(ListViewHandle, Index);
}

VOID PhSetListViewSubItem(
    __in HWND ListViewHandle,
    __in INT Index,
    __in INT SubItemIndex,
    __in PWSTR Text
    )
{
    LVITEM item;

    item.mask = LVIF_TEXT;
    item.iItem = Index;
    item.iSubItem = SubItemIndex;
    item.pszText = Text;

    ListView_SetItem(ListViewHandle, &item);
}

HWND PhCreateTabControl(
    __in HWND ParentHandle
    )
{
    HWND tabControlHandle;

    tabControlHandle = CreateWindow(
        WC_TABCONTROL,
        L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0,
        0,
        3,
        3,
        ParentHandle,
        NULL,
        PhInstanceHandle,
        NULL
        );

    if (tabControlHandle)
    {
        SendMessage(tabControlHandle, WM_SETFONT, (WPARAM)PhApplicationFont, FALSE);
    }

    return tabControlHandle;
}

INT PhAddTabControlTab(
    __in HWND TabControlHandle,
    __in INT Index,
    __in PWSTR Text
    )
{
    TCITEM item;

    item.mask = TCIF_TEXT;
    item.pszText = Text;

    return TabCtrl_InsertItem(TabControlHandle, Index, &item);
}

PPH_STRING PhGetWindowText(
    __in HWND hwnd
    )
{
    PPH_STRING string; 
    ULONG length;

    length = GetWindowTextLength(hwnd);

    if (length == 0)
        return PhCreateString(L"");

    string = PhCreateStringEx(NULL, length * 2);

    if (GetWindowText(hwnd, string->Buffer, string->Length / 2 + 1))
    {
        return string;
    }
    else
    {
        PhDereferenceObject(string);
        return NULL;
    }
}

VOID PhShowContextMenu(
    __in HWND hwnd,
    __in HWND subHwnd,
    __in HMENU menu,
    __in POINT point
    )
{
    ClientToScreen(subHwnd, &point);

    TrackPopupMenu(
        menu,
        TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
        point.x,
        point.y,
        0,
        hwnd,
        NULL
        );
}

VOID PhInitializeLayoutManager(
    __out PPH_LAYOUT_MANAGER Manager,
    __in HWND RootWindowHandle
    )
{
    Manager->List = PhCreateList(4);
    Manager->LayoutNumber = 0;

    Manager->RootItem.Handle = RootWindowHandle;
    GetClientRect(Manager->RootItem.Handle, &Manager->RootItem.Rect);
    Manager->RootItem.ParentItem = NULL;
    Manager->RootItem.LayoutNumber = 0;
    Manager->RootItem.NumberOfChildren = 0;
    Manager->RootItem.DeferHandle = NULL;
}

VOID PhDeleteLayoutManager(
    __inout PPH_LAYOUT_MANAGER Manager
    )
{
    ULONG i;

    for (i = 0; i < Manager->List->Count; i++)
        PhFree(Manager->List->Items[i]);

    PhDereferenceObject(Manager->List);
}

VOID PhpConvertRect(
    __inout PRECT Rect,
    __in PRECT ParentRect
    )
{
    Rect->right = ParentRect->right - ParentRect->left - Rect->right;
    Rect->bottom = ParentRect->bottom - ParentRect->top - Rect->bottom;
}

PPH_LAYOUT_ITEM PhAddLayoutItem(
    __inout PPH_LAYOUT_MANAGER Manager,
    __in HWND Handle,
    __in PPH_LAYOUT_ITEM ParentItem,
    __in ULONG Anchor
    )
{
    PPH_LAYOUT_ITEM item;

    if (!ParentItem)
        ParentItem = &Manager->RootItem;

    item = PhAllocate(sizeof(PH_LAYOUT_ITEM));
    item->Handle = Handle;
    item->ParentItem = ParentItem;
    item->LayoutNumber = Manager->LayoutNumber;
    item->NumberOfChildren = 0;
    item->DeferHandle = NULL;
    item->Anchor = Anchor;

    item->ParentItem->NumberOfChildren++;

    GetWindowRect(Handle, &item->Rect);
    MapWindowPoints(NULL, item->ParentItem->Handle, (POINT *)&item->Rect, 2);

    item->Margin = item->Rect;
    PhpConvertRect(&item->Margin, &item->ParentItem->Rect);

    item->OrigRect = item->Rect;

    PhAddListItem(Manager->List, item);

    return item;
}

VOID PhpLayoutItemLayout(
    __inout PPH_LAYOUT_MANAGER Manager,
    __inout PPH_LAYOUT_ITEM Item
    )
{
    RECT rect;

    if (Item->NumberOfChildren > 0 && !Item->DeferHandle)
        Item->DeferHandle = BeginDeferWindowPos(Item->NumberOfChildren);

    if (Item->LayoutNumber == Manager->LayoutNumber)
        return;

    // If this is the root item we must stop here.
    if (!Item->ParentItem)
        return;

    PhpLayoutItemLayout(Manager, Item->ParentItem);

    GetWindowRect(Item->Handle, &Item->Rect);
    MapWindowPoints(NULL, Item->ParentItem->Handle, (POINT *)&Item->Rect, 2);

    // Convert right/bottom into margins to make the calculations 
    // easier.
    rect = Item->Rect;
    PhpConvertRect(&rect, &Item->ParentItem->Rect);

    if (!(Item->Anchor & (PH_ANCHOR_LEFT | PH_ANCHOR_RIGHT)))
    {
        // TODO
        PhRaiseStatus(STATUS_NOT_IMPLEMENTED);
    }
    else if (Item->Anchor & PH_ANCHOR_RIGHT)
    {
        if (Item->Anchor & PH_ANCHOR_LEFT)
        {
            rect.left = Item->Margin.left;
            rect.right = Item->Margin.right;
        }
        else
        {
            ULONG diff = Item->Margin.right - rect.right;

            rect.left -= diff;
            rect.right += diff;
        }
    }

    if (!(Item->Anchor & (PH_ANCHOR_TOP | PH_ANCHOR_BOTTOM)))
    {
        // TODO
        PhRaiseStatus(STATUS_NOT_IMPLEMENTED);
    }
    else if (Item->Anchor & PH_ANCHOR_BOTTOM)
    {
        if (Item->Anchor & PH_ANCHOR_TOP)
        {
            rect.top = Item->Margin.top;
            rect.bottom = Item->Margin.bottom;
        }
        else
        {
            ULONG diff = Item->Margin.bottom - rect.bottom;

            rect.top -= diff;
            rect.bottom += diff;
        }
    }

    // Convert the right/bottom back into co-ordinates.
    PhpConvertRect(&rect, &Item->ParentItem->Rect);
    Item->Rect = rect;

    Item->ParentItem->DeferHandle = DeferWindowPos(
        Item->ParentItem->DeferHandle, Item->Handle,
        NULL, rect.left, rect.top,
        rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOACTIVATE | SWP_NOZORDER
        );

    Item->LayoutNumber = Manager->LayoutNumber;
}

VOID PhLayoutManagerLayout(
    __inout PPH_LAYOUT_MANAGER Manager
    )
{
    ULONG i;

    Manager->LayoutNumber++;

    GetClientRect(Manager->RootItem.Handle, &Manager->RootItem.Rect);

    for (i = 0; i < Manager->List->Count; i++)
    {
        PPH_LAYOUT_ITEM item = (PPH_LAYOUT_ITEM)Manager->List->Items[i];

        PhpLayoutItemLayout(Manager, item);
    }

    for (i = 0; i < Manager->List->Count; i++)
    {
        PPH_LAYOUT_ITEM item = (PPH_LAYOUT_ITEM)Manager->List->Items[i];

        if (item->DeferHandle)
        {
            EndDeferWindowPos(item->DeferHandle);
            item->DeferHandle = NULL;
        }
    }

    if (Manager->RootItem.DeferHandle)
    {
        EndDeferWindowPos(Manager->RootItem.DeferHandle);
        Manager->RootItem.DeferHandle = NULL;
    }
}
