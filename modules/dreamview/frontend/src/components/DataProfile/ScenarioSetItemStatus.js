import React from 'react';

import { PLUGIN_WS } from 'store/websocket';
import { Tooltip } from 'antd';

import ProfileUpdatingIcon from 'assets/images/icons/profile_updating.png';
import ProfileSuccessIcon from 'assets/images/icons/profile_success.png';
import ProfileFailIcon from 'assets/images/icons/profile_fail.png';

/**
 * Show remote scene set download status
 * @param props {{
 * status: "notDownloaded" | "toBeUpdated" | "updating" | "downloaded" | "fail",
 * errorMsg: string | undefined
 * }}
 * @returns {JSX.Element|null}
 */
export function ScenarioSetItemStatus(props) {
  const { status } = props;

  if (status === 'notDownloaded') {
    return (
      <div className='scenario-set-list-item_status'>
        <span>Not downloaded</span>
      </div>
    );
  }

  if (status === 'toBeUpdated') {
    return (
      <div className='scenario-set-list-item_status'>
        <span>To be updated</span>
      </div>
    );
  }

  if (status === 'updating') {
    return (
      <div className='scenario-set-list-item_status'>
        <img className='scenario-set-list-item_status_icon scenario-set-list-item_status_updating'
             src={ProfileUpdatingIcon} alt='updating' />
        <span>Updating……</span>
      </div>
    );
  }

  if (status === 'downloaded') {
    return (
      <div className='scenario-set-list-item_status'>
        <img className='scenario-set-list-item_status_icon' src={ProfileSuccessIcon} alt='downloaded' />
        <span className='scenario-set-list-item_status_downloaded'>Downloaded</span>
      </div>
    );
  }

  if (status === 'fail') {
    return (
      <div className='scenario-set-list-item_status'>
        <Tooltip
          placement='topLeft'
          title={props.errorMsg}>
          <img className='scenario-set-list-item_status_icon' src={ProfileFailIcon} alt='fail' />
          <span className='scenario-set-list-item_status_fail'>Fail</span>
        </Tooltip>
      </div>
    );
  }

  return null;
}

/**
 * Show remote scene set download status
 * @param props {{
 * status: "notDownloaded" | "toBeUpdated" | "updating" | "downloaded" | "fail",
 * scenarioSetId: string,
 * store: object,
 * }}
 * @returns {JSX.Element|null}
 */
export function ScenarioSetItemBtn(props) {
  const { status, scenarioSetId, store } = props;

  const handleClick = () => {
    if (status === 'updating') {
      store.studioConnector.updatingTheScenarioSetById(scenarioSetId);
    }
    PLUGIN_WS.downloadScenarioSetById(scenarioSetId);
  };

  if (status === 'notDownloaded') {
    return (
      <div className='scenario-set-list-item_button' onClick={handleClick}>Download</div>
    );
  }

  if (status === 'toBeUpdated') {
    return (
      <div className='scenario-set-list-item_button' onClick={handleClick}>Update</div>
    );
  }

  if (status === 'updating') {
    return (
      <div className='scenario-set-list-item_button' onClick={handleClick}>Cancel</div>
    );
  }

  if (status === 'downloaded') {
    return <div className='scenario-set-list-item_button scenario-set-list-item_button_placehold'></div>;
  }

  if (status === 'fail') {
    return (
      <div className='scenario-set-list-item_button' onClick={handleClick}>Retry</div>
    );
  }

  return null;
}
