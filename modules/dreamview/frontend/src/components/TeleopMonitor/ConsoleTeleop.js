import React from "react";
import { inject, observer } from "mobx-react";

import { TELEOP_WS } from "store/websocket";

import CheckboxItem from "components/common/CheckboxItem";
import MonitorSection from "components/TeleopMonitor/MonitorSection";

import itemIcon from "assets/images/icons/teleop_item.png";

function MicIcon(props) {
    const color = props.isOn ? "#30A5FF" : "#999999";

    return (
        <svg className="mic" version="1.1" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32" width="20" height="20">
            <defs>
                <path id="oval" d="M17.81 0.27L18.64 0.58L19.41 1.01L20.11 1.54L20.73 2.16L21.26 2.86L21.69 3.63L22.01 4.46L22.21 5.35L22.27 6.27L22.27 14.78L22.21 15.7L22.01 16.59L21.69 17.42L21.26 18.19L20.73 18.89L20.11 19.51L19.41 20.04L18.64 20.46L17.81 20.78L16.93 20.98L16 21.05L15.08 20.98L14.2 20.78L13.36 20.47L12.59 20.04L11.89 19.51L11.27 18.89L10.74 18.19L10.32 17.42L10 16.59L9.8 15.7L9.73 14.78L9.73 6.27L9.8 5.35L10 4.46L10.32 3.63L10.74 2.86L11.27 2.16L11.89 1.54L12.59 1.01L13.36 0.58L14.2 0.27L15.08 0.07L16 0L16.93 0.07L17.81 0.27ZM14.88 1.83L14.34 2.12L13.81 2.52L13.29 3.04L12.78 3.67L12.29 4.42L11.8 5.29L11.33 6.27L11.33 14.78L11.4 15.62L11.62 16.41L11.97 17.13L12.43 17.79L12.99 18.35L13.65 18.81L14.37 19.16L15.16 19.38L16 19.45L16.84 19.38L17.63 19.16L18.36 18.81L19.01 18.35L19.58 17.79L20.04 17.13L20.38 16.41L20.6 15.62L20.68 14.78L20.68 6.27L20.6 5.43L20.38 4.64L20.04 3.91L19.58 3.26L19.01 2.7L18.36 2.24L17.63 1.89L16.84 1.67L16 1.6L15.44 1.65L14.88 1.83Z" />
                <path id="bottom" d="M5.35 14.78C5.35 20.38 9.7 24.99 15.2 25.4C15.2 25.73 15.2 27.4 15.2 30.4C12.7 30.4 11.31 30.4 11.04 30.4C10.59 30.4 10.23 30.75 10.23 31.2C10.23 31.64 10.59 32 11.04 32C12.03 32 19.97 32 20.96 32C21.41 32 21.77 31.64 21.77 31.2C21.77 30.75 21.41 30.4 20.96 30.4C20.69 30.4 19.3 30.4 16.8 30.4C16.8 27.4 16.8 25.73 16.8 25.4C22.3 24.99 26.65 20.38 26.65 14.78C26.65 14.34 26.29 13.98 25.85 13.98C25.4 13.98 25.04 14.34 25.04 14.78C25.04 19.78 20.98 23.84 15.99 23.84C11 23.84 6.94 19.78 6.94 14.78C6.95 14.34 6.6 13.98 6.15 13.98C5.71 13.98 5.35 14.34 5.35 14.78Z" />
                <path id="mute" d="M29.02 2.98L2.08 23.36" />
            </defs>
            <use xlinkHref="#oval" opacity="1" fillOpacity="1" fill={color} />
            <use xlinkHref="#bottom" opacity="1" fillOpacity="1" fill={color} />
            <use xlinkHref="#mute" opacity={props.isOn ? "0" : "1"} fillOpacity="0" stroke={color} strokeWidth="2" strokeOpacity="1" />
        </svg >
    );
};

function OperationButton(props) {
    const { name, command } = props;

    return (
        <button
            className="command-button teleop-button"
            onClick={() => {
                if (confirm(`Are you sure you want to execute ${name} command?`)) {
                    command();
                }
            }}
        >
            {name}
        </button>
    );
}

@inject("store") @observer
export default class ConsoleTeleOp extends React.Component {
    constructor(props) {
        super(props);

        this.operation = {
            "STOP": () => {
                TELEOP_WS.executeCommand("EStop");
            },
            "PULL OVER": () => {
                TELEOP_WS.executeCommand("PullOver");
            },
            "RESUME": () => {
                TELEOP_WS.executeCommand("ResumeCruise");
            }
        };
    }

    componentDidMount() {
        TELEOP_WS.initialize();
    }

    componentWillUnmount() {
        TELEOP_WS.close();
    }

    render() {
        const { teleop } = this.props.store;
        const audioDisabled = !teleop.audioEnabled;
        const micOn = teleop.micEnabled;

        return (
            <div className="monitor teleop">
                <div className="monitor-header">
                    <div className="title">Console Teleop Controls</div>
                </div>

                <div className="monitor-content">
                    <MonitorSection title="Audio" icon={itemIcon}>
                        <div className="section-content-row" key={name}>
                            <CheckboxItem id='teleopAudio'
                                          title={"On/Off"}
                                          extraClasses="field"
                                          isChecked={teleop.audioEnabled}
                                          onClick={() => {
                                            TELEOP_WS.executeCommand('ToggleAudio');
                                            teleop.toggleAudio();
                                          }} />
                            <button className="field mute-button"
                                    disabled={audioDisabled}
                                    onClick={() => {
                                        teleop.toggleMic();
                                        TELEOP_WS.executeCommand('ToggleMic');
                                    }} >
                                    <MicIcon isOn={micOn} />
                            </button>
                        </div>
                    </MonitorSection>
                    <MonitorSection title="Video" icon={itemIcon}>
                        <CheckboxItem id='teleopVideo'
                                      title={"On/Off"}
                                      isChecked={teleop.videoEnabled}
                                      onClick={() => {
                                        TELEOP_WS.executeCommand('ToggleVideo');
                                        teleop.toggleVideo();
                                      }} />
                    </MonitorSection>
                    <MonitorSection title="Signal" icon={itemIcon}>
                        {teleop.modemInfo.entries().map(([name, value]) => (
                                <div className="section-content-row" key={name}>
                                    <span className="field">{name}</span>
                                    <span className="field">{value}</span>
                                </div>
                            ))
                        }
                    </MonitorSection>
                    <MonitorSection title="Operation" icon={itemIcon}>
                        <div className="teleop-command-group">
                            {Object.entries(this.operation).map(([name, command]) => (
                                <OperationButton key={name} name={name} command={command} />)
                            )}
                        </div>
                    </MonitorSection>
                </div>
            </div >
        );
    }
}
