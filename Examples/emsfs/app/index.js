
const React = require('react')
const ReactDOM = require('react-dom')
const { useSpring, animated } = require('react-spring')

const calc = (x, y) => [-(y - window.innerHeight / 2) / 20, (x - window.innerWidth / 2) / 20, 1.1]
const trans = (x, y, s) => `perspective(600px) rotateX(${x}deg) rotateY(${y}deg) scale(${s})`

function Card() {
  const [props, set] = useSpring({ xys: [0, 0, 1], config: { mass: 5, tension: 350, friction: 40 } })
  return (
    React.createElement(animated.div, {
        className: "card",
        style:{ transform: props.xys.interpolate(trans) },
        onMouseMove({ clientX: x, clientY: y }) {
            return set({ xys: calc(x, y) })
        },
        onMouseLeave() {
           return set({ xys: [0, 0, 1] })
        },
    })
  )
}

ReactDOM.render(React.createElement(Card), document.getElementById('mount'))
