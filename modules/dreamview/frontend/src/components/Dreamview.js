import React from "react";
import { inject, observer } from "mobx-react";
import SplitPane from 'react-split-pane';

import DashCamPlayer from "components/DashCamPlayer";
import SideBar from "components/SideBar";
import StatusBar from "components/StatusBar";
import Scene from "components/Scene";
import RouteEditingBar from "components/RouteEditingBar";
import Loader from "components/common/Loader";
import PNCMonitor from "components/PNCMonitor";
import WS from "store/websocket";


@inject("store") @observer
export default class Dreamview extends React.Component {
    constructor(props) {
        super(props);
        this.handleDrag = this.handleDrag.bind(this);
    }

    handleDrag(masterViewWidth) {
        const { options } = this.props.store;
        if (options.showPNCMonitor) {
            this.props.store.updateWidthInPercentage(
                Math.min(1.00, masterViewWidth / window.innerWidth));
        }
    }

    componentDidMount() {
        WS.initialize();
        window.addEventListener("resize", () => {
            this.props.store.updateDimension();
        });
    }

    render() {
        const { isInitialized, dimension, meters, options,
                video, routeEditingManager } = this.props.store;

        if (!isInitialized) {
            return (<Loader />);
        }

        const showBars = !routeEditingManager.inEditingView;
        const showRoutingBar = routeEditingManager.inEditingView;
        const showVideo = (video.path !== undefined && video.path.length !== 0);

        return (
            <SplitPane split="vertical"
                       size={dimension.width}
                       onChange={this.handleDrag}
                       allowResize={options.showPNCMonitor}>
                <div>
                    {showBars ? <SideBar /> : null}
                    {showBars ? <StatusBar meters={meters} /> : null}
                    {showRoutingBar ? <RouteEditingBar /> : null}
                    <Scene
                        width={dimension.width}
                        height={dimension.height}
                        options={options}
                        invisible={false}/>
                    {showVideo  ? <DashCamPlayer /> : null}
                </div>
                {options.showPNCMonitor ? <PNCMonitor /> : <div></div>}
            </SplitPane>
        );
    }
}