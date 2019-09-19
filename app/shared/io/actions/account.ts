import * as ACCOUNT_API from '__io/db/account';
import { TD_DIR, buildGatewayPath } from '__gConfig/pathConfig';
import { removeFileFolder } from '__gUtils/fileUtils';
import { startTd, startMd, deleteProcess } from '__gUtils/processUtils';

const path = require('path')

//删除账户需要将所关联的数据库以及进程都关掉
//判断task表和进程中是否存在，有则删除
//TODO
export const deleteAccount = (row: Account, accountList = []): Promise<any> => {
    const { account_id, source_name, receive_md } = row
    //查看该账户下是否存在task中的td任务
    const tdProcessId: string = `td_${account_id}`
    const mdProcessId: string = `md_${source_name}`
    const leftAccounts: Account[] = accountList.filter((a: Account): boolean => {
        return (a.source_name === source_name) && (!!a.receive_md === false) && (a.account_id !== account_id)
    })
    //删除td
    return removeFileFolder(path.join(TD_DIR, account_id.toAccountId()))
    .then(() => removeFileFolder(buildGatewayPath(tdProcessId)))
    .then(() => deleteProcess('td_' + row.account_id))
    .then(() => {if(receive_md) return removeFileFolder(buildGatewayPath(mdProcessId))})
    .then(() => {if(receive_md) return deleteProcess('md_' + row.source_name)})                     
    .then(() => ACCOUNT_API.deleteAccount(account_id))//删除账户表中的数据    
    .then(() => {if(receive_md && leftAccounts.length) return ACCOUNT_API.changeAccountMd(leftAccounts[0].account_id, true)})
}

//起停td
export const switchTd = (account: Account, value: boolean): Promise<any> => {
    const { account_id, config } = account
    const tdProcessId: string = `td_${account_id}`
    if(!value){
        return deleteProcess(tdProcessId)
        .then((): MessageData => ({ type: 'success', message: '操作成功！' }))       
        .catch((err: Error): MessageData => ({ type: 'error', message: err.message || '操作失败！' }))
    }

    //改变数据库表内容，添加或修改
    return startTd(account_id)
    .then((): MessageData => ({ type: 'start', message: '正在启动...' }))       
    .catch((err: Error): MessageData => ({ type: 'error', message: err.message || '操作失败！' }))
}

//起停md
export const switchMd = (account: Account, value: boolean) => {
    const { source_name, config } = account;
    const mdProcessId: string = `md_${source_name}`
    if(!value){
        return deleteProcess(mdProcessId)
        .then((): MessageData => ({ type: 'success', message: '操作成功！' }))       
        .catch((err: Error): MessageData => ({ type: 'error', message: err.message || '操作失败！' }))
    }

    //改变数据库表内容，添加或修改
    return startMd(source_name)
    .then((): MessageData => ({ type: 'start', message: '正在启动...' }))       
    .catch((err: Error): MessageData => ({ type: 'error', message: err.message || '操作失败！' }))     
}