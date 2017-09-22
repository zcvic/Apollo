import React from "react";
import { observer } from "mobx-react";
import classNames from "classnames";
import menuData from 'store/config/MenuData';
import perceptionIcon from "assets/images/menu/Perception.png";
import predictionIcon from "assets/images/menu/Prediction.png";
import routingIcon from "assets/images/menu/Routing.png";
import decisionIcon from "assets/images/menu/Decision.png";
import planningIcon from "assets/images/menu/Planning.png";
import cameraIcon from "assets/images/menu/PointOfView.png";

const MenuIconMapping = {
        perception: perceptionIcon,
        prediction: predictionIcon,
        routing: routingIcon,
        decision: decisionIcon,
        planning: planningIcon,
        camera: cameraIcon
};

const MenuIdOptionMapping = {
        perceptionVehicle: 'showObstaclesVehicle',
        perceptionPedestrian: 'showObstaclesPedestrian',
        perceptionBicycle: 'showObstaclesBicycle',
        perceptionUnknownMovable: 'showObstaclesUnknownMovable',
        perceptionUnknownUnmovable: 'showObstaclesUnknownUnmovable',
        perceptionUnknown: 'showObstaclesUnknown',
        perceptionVirtual: 'showObstaclesVirtual',
        perceptionVelocity: 'showObstaclesVelocity',
        perceptionHeading: 'showObstaclesHeading',
        perceptionId: 'showObstaclesId',
        predictionMajor: 'showPredictionMajor',
        predictionMinor: 'showPredictionMinor',
        routing: 'showRouting',
        decisionMain: 'showDecisionMain',
        decisionObstacle: 'showDecisionObstacle',
        planningLine: 'showPlanning'
};

@observer
class MenuItemCheckbox extends React.Component {
    render() {
        const {id, title, options} = this.props;
        return (
            <ul>
                <li id={id} onClick={() => {
                    options.toggle(MenuIdOptionMapping[id]);
                }}>
                    <div className="switch">
                        <input type="checkbox" name={id} className="toggle-switch"
                        id={id} checked={options[MenuIdOptionMapping[id]]} readOnly/>
                        <label className="toggle-switch-label" htmlFor={id} />
                    </div>
                    <span>  {title}</span>
                </li>
            </ul>
        );
    }
}

@observer
class MenuItemRadio extends React.Component {
    render() {
        const {id, title, options} = this.props;
        return (
            <ul>
                <li id={title} onClick={() => {
                    options.selectCamera(title);
                }}>
                    <input type="radio" name={id} id={title}
                    checked={options.cameraAngle === title} readOnly/>
                    <label id="radio-selector-label" htmlFor={title} />
                    <span>  {title}</span>
                </li>
            </ul>
        );
    }
}

@observer
class SubMenu extends React.Component {
    render() {
        const {tabId, tabTitle, tabType, data, options} = this.props;
        let entries = null;
        if (tabType === 'checkbox') {
            entries = Object.keys(data)
                .map(key => {
                    const item = data[key];
                    return (
                        <MenuItemCheckbox key={key} id={key} title={item}
                        options={options}/>
                    );
                });
        } else if (tabType === 'radio') {
            entries = Object.keys(data)
                .map(key => {
                    const item = data[key];
                    return (
                        <MenuItemRadio key={`${tabId}_${key}`} id={tabId}
                        title={item} options={options}/>
                    );
                });
        }
        const result = (
            <div>
                <details>
                    <summary><img src={MenuIconMapping[tabId]} />
                    <span> {tabTitle}</span></summary>
                    {entries}
                </details>
            </div>
        );
        return result;
    }
}

@observer
export default class Menu extends React.Component {
    render() {
        const { options } = this.props;
        const subMenu = Object.keys(menuData)
            .map(key => {
                const item = menuData[key];
                return (
                    <SubMenu key={item.id} tabId={item.id} tabTitle={item.title}
                    tabType={item.type} data={item.data} options={options} />
                );
            });

        return (
            <div className="nav-side-menu" id="layer-menu">
                {subMenu}
            </div>
        );
    }
}
